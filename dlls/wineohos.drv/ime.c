#if 0
#pragma makedep unix
#endif

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#include "ohos.h"
#include "wine/debug.h"
#include "ohos_adapter.h"

WINE_DEFAULT_DEBUG_CHANNEL(ohos);

pthread_mutex_t ime_mutex;
static bool ime_enabled = false;
static HWND ime_focused_hwnd = 0;
static struct ime_cursor_info g_cursor_info = {0};

static void lock_ime_state()
{
    pthread_mutex_lock( &ime_mutex );
}

static void unlock_ime_state()
{
    pthread_mutex_unlock( &ime_mutex );
}

static void post_ime_update( HWND hwnd, UINT cursor_pos, WCHAR *comp_str, WCHAR *result_str )
{
    NtUserMessageCall( hwnd, WINE_IME_POST_UPDATE, cursor_pos, (LPARAM)comp_str,
                       result_str, NtUserImeDriverCall, FALSE );
}

static void on_ime_commit_text(const char* str, int count)
{
    WCHAR *output;
    DWORD len;

    if (!(output = malloc( (count + 1) * sizeof(WCHAR) ))) return;
    len = ntdll_umbstowcs( str, count, output, count );
    output[len] = 0;

    TRACE( "ime commit text, ime_focused_hwnd : %d, string : %s\n", ime_focused_hwnd, debugstr_an(str, count) );
    post_ime_update( ime_focused_hwnd, 0, NULL, output );

    free( output );
}

static void on_ime_delete_text(int direction, int32_t length)
{
    WORD vkey;
    WORD scan;
    if (direction == OHOS_IME_DELETE_BACKWARD) {
        vkey = VK_BACK;
        scan = 0x0e;
    } else if (direction == OHOS_IME_DELETE_FORWARD) {
        vkey = VK_DELETE;
        scan = 0x153;
    }

    TRACE("delete text, ime_focused_hwnd : %d direction : %d\n", ime_focused_hwnd, direction);
    send_keyboard_input( ime_focused_hwnd, vkey, scan, 0 );
}

static void on_ime_function_key(const int enter_key_type)
{
    TRACE("enter_key hwnd : %d, type : %d\n", ime_focused_hwnd, enter_key_type);
    send_keyboard_input( ime_focused_hwnd, VK_RETURN, 0x1c, 0 );
}

void send_ime_event( const ime_event* event )
{
    union event_data data;

    data.type = IME_EVENT;
    data.ime.data.ime_type = event->type;

    int size = event->data.input_text_len;
    data.ime.data.input_text_len = size;
    if (event->data.input_text) {
        memcpy(data.ime.data.input_text, event->data.input_text, size + 1);
    }
    data.ime.data.delete_text_len = event->data.delete_text_len;
    data.ime.data.move_direction = event->data.move_direction;
    data.ime.data.enter_key_type = event->data.enter_key_type;

    send_event( &data );
    return;
}

void distribute_ime_callback( const struct ime_data* data )
{
    TRACE("distribute_ime_callback, callback type : %d\n", data->ime_type);
    lock_ime_state();
    if (!ime_enabled || !ime_focused_hwnd) {
        unlock_ime_state();
        return;
    }
    switch (data->ime_type) {
        case OHOS_IME_INSERT_TEXT:
            on_ime_commit_text(data->input_text, data->input_text_len);
            break;
        case OHOS_IME_DELETE_BACKWARD:
            on_ime_delete_text(OHOS_IME_DELETE_BACKWARD, data->delete_text_len);
            break;
        case OHOS_IME_DELETE_FORWARD:
            on_ime_delete_text(OHOS_IME_DELETE_FORWARD, data->delete_text_len);
            break;
        case OHOS_IME_SEND_FUNCTION_KEY:
            on_ime_function_key(data->enter_key_type);
            break;
        default:
            break;
    }
    unlock_ime_state();
}

void init_input_attribute(struct ime_input_attribute* input_attribute)
{
    LONG style = NtUserGetWindowLongW(ime_focused_hwnd, GWL_STYLE);
    if (style & ES_PASSWORD) {
        if (style & ES_NUMBER) {
            input_attribute->input_pattern = IME_TEXT_INPUT_NUMBER_PASSWORD;
        } else {
            input_attribute->input_pattern = IME_TEXT_INPUT_VISIBLE_PASSWORD;
        }
    } else if (style & ES_NUMBER) {
        input_attribute->input_pattern = IME_TEXT_INPUT_NUMBER_DECIMAL;
    } else if (style & ES_MULTILINE) {
        input_attribute->input_pattern = IME_TEXT_INPUT_MULTILINE;
        if (style & ES_WANTRETURN) {
            input_attribute->enter_key_type = IME_ENTER_KEY_NEWLINE;
        }
    }
}

void update_input_attribute()
{
    struct ime_input_attribute input_attribute = {0};
    init_input_attribute(&input_attribute);
    IMEUpdateAttribute(input_attribute);
}

void ime_attach()
{
    bool showKeyboard = false;
    int reason = IME_REQUEST_KEYBOARD_NONE;
    struct ime_text_config text_config = {0};
    struct ime_input_attribute input_attribute = {0};

    input_attribute.input_pattern = IME_TEXT_INPUT_TEXT;
    input_attribute.enter_key_type = IME_ENTER_KEY_DONE;
    init_input_attribute(&input_attribute);

    text_config.input_attribute = input_attribute;
    text_config.cursor_info = g_cursor_info;
    text_config.range.start = 0;
    text_config.range.end = 0;

    TRACE("ime attach\n");
    IMEAttachTextInput(showKeyboard, &text_config, reason);
}

void OHOS_NotifyIMEStatus(HWND hwnd, UINT status)
{
    TRACE("notify ime status, hwnd : %d, status : %u\n", hwnd, status);
    struct ohos_win_data *data;
    if (!(data = get_win_data(hwnd))) return;

    lock_ime_state();
    if (status) {
        if (!ime_enabled) {
            ime_attach();
            ime_enabled = true;
        } else {
            update_input_attribute();
        }
        ime_focused_hwnd = hwnd;
    } else {
        if (ime_focused_hwnd == hwnd) {
            TRACE("ime detach\n");
            IMEDetachTextInput();
            ime_enabled = false;
            ime_focused_hwnd = 0;
        }
    }
    release_win_data(data);
    unlock_ime_state();
}

BOOL OHOS_SetIMECompositionRect(HWND hwnd, RECT rect)
{
    TRACE("set ime composition rect, hwnd : %d, rect : {%d,%d,%d,%d}\n",
          hwnd, rect.left, rect.top, rect.right, rect.bottom);
    
    g_cursor_info.left = rect.left;
    g_cursor_info.top = rect.top;
    g_cursor_info.width = rect.right - rect.left;
    g_cursor_info.height = rect.bottom - rect.top;
    IMENotifyCursorUpdate(&g_cursor_info);

    return TRUE;
}

void ime_on_screen_unlock(void)
{
    lock_ime_state();
    if (ime_enabled && ime_focused_hwnd)
    {
        TRACE("screen unlocked, re-attaching ime\n");
        ime_attach();
    }
    unlock_ime_state();
}
