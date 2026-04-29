#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <ace/xcomponent/native_xcomponent_key_event.h>
#include <multimodalinput/oh_input_manager.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"
#include "ddk/wdm.h"
#include "ohos.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ohos);

static HWND capture_window;

#define NB_CACHED_BUFFERS 4
#define HAS_FLAGS(flags, required) (((flags) & (required)) == (required))

struct native_win_data
{
    ohos_window             win;
    struct native_buffer   *buffers[NB_CACHED_BUFFERS];
    struct native_buffer   *locked_buffer;
    HWND                    hwnd;
    HWND                    parent;
    int                     generation;
    int                     buffer_format;
    int                     swap_interval;
    int                     buffer_lru[NB_CACHED_BUFFERS];
    int                     fence_id[NB_CACHED_BUFFERS];
    int                     widget_id;
    int                     container;
    int                     win_type;
};

static struct native_win_data *data_map[65536];

static unsigned int data_map_idx( HWND hwnd )
{
    return LOWORD(hwnd);
}

static struct native_win_data *get_native_win_data( HWND hwnd )
{
    struct native_win_data *data = data_map[data_map_idx( hwnd )];

    if (data && data->hwnd == hwnd) return data;
    WARN( "unknown win %p\n", hwnd );
    return NULL;
}

static HWND get_hwnd_by_widget_id( const int widget_id )
{
    for (int i = 0; i < 65536; ++i) {
        if (data_map[i] && data_map[i]->widget_id == widget_id)
            return data_map[i]->hwnd;
    }
    return NULL;
}

static int get_win_parent( HWND parent )
{
    if (parent != NtUserGetDesktopWindow() && !NtUserGetAncestor( parent, GA_PARENT ))
        return HandleToLong( HWND_MESSAGE );
    return HandleToLong( parent );
}

static void release_native_window( struct native_win_data *data )
{
    unsigned int i;

    for (i = 0; i < NB_CACHED_BUFFERS; i++)
    {
        if (data->buffers[i]) OH_NativeWindow_NativeObjectUnreference(data->buffers[i]->win_buffer);
        data->buffer_lru[i] = -1;
    }
    memset( data->buffers, 0, sizeof(data->buffers) );
}

static void free_native_win_data( struct native_win_data *data )
{
    unsigned int idx = data_map_idx( data->hwnd );

    InterlockedCompareExchangePointer( (void **)&capture_window, 0, data->hwnd );
    release_native_window( data );
    free( data );
    data_map[idx] = NULL;
}

static struct native_win_data *create_native_win_data( HWND hwnd )
{
    unsigned int i, idx = data_map_idx( hwnd );
    struct native_win_data *data = data_map[idx];

    if (data)
    {
        WARN( "data for %p not freed correctly\n", data->hwnd );
        free_native_win_data( data );
    }
    if (!(data = calloc( 1, sizeof(*data) ))) return NULL;
    data->hwnd = hwnd;
    data->buffer_format = NATIVEBUFFER_PIXEL_FMT_BGRA_8888;
    data->parent = NtUserGetAncestor( hwnd, GA_PARENT );
    data_map[idx] = data;
    for (i = 0; i < NB_CACHED_BUFFERS; i++) data->buffer_lru[i] = -1;
    return data;
}

static struct window_features get_window_features( HWND hwnd )
{
    DWORD style, ex_style;

    style = NtUserGetWindowLongW(hwnd, GWL_STYLE);
    ex_style = NtUserGetWindowLongW(hwnd, GWL_EXSTYLE);

    return get_window_features_for_style(style, ex_style);
}

static int get_owner_window_id( HWND hwnd )
{
    int widget_id = -1;
    HWND owner;
    struct native_win_data *owner_win;
    struct ohos_win_data *data = get_win_data( hwnd );
    if (!data)
        return -1;

    owner = NtUserGetWindowRelative(hwnd, GW_OWNER);
    /* fallback to any window that is right below our top left corner */
    if (!owner) owner = NtUserWindowFromPoint(data->rects.window.left - 1, data->rects.window.top - 1);
    if (owner) owner = NtUserGetAncestor(owner, GA_ROOT);
    if (!(owner_win = get_native_win_data( owner )))
        goto done;

    widget_id = owner_win->widget_id;

done:
    release_win_data( data );
    return widget_id;
}

struct window_features get_window_features_for_style(DWORD style, DWORD ex_style)
{
    struct window_features wf = {0};

    if (ex_style & WS_EX_NOACTIVATE) wf.prevents_app_activation = TRUE;

    if ((style & WS_CAPTION) == WS_CAPTION && !(ex_style & WS_EX_LAYERED))
    {
        wf.shadow = TRUE;
        wf.title_bar = TRUE;
        if (style & WS_SYSMENU) wf.close_button = TRUE;
        if (style & WS_MINIMIZEBOX) wf.minimize_button = TRUE;
        if (style & WS_MAXIMIZEBOX) wf.maximize_button = TRUE;
    }
    if (style & WS_THICKFRAME)
    {
        wf.shadow = TRUE;
        wf.resizable = TRUE;
    }
    else if (ex_style & WS_EX_DLGMODALFRAME) wf.shadow = TRUE;
    else if ((style & (WS_DLGFRAME | WS_BORDER)) == WS_DLGFRAME) wf.shadow = TRUE;

    wf.use_floating_window = FALSE;
    wf.use_dark_mode = FALSE;
    return wf;
}

int get_widget_id( HWND hwnd )
{
    struct native_win_data *data = data_map[data_map_idx( hwnd )];

    if (data && data->hwnd == hwnd) return data->widget_id;
    WARN( "unknown win %p\n", hwnd );
    return -1;
}

/* get the capture window stored in the desktop process */
HWND get_capture_window(void)
{
    return capture_window;
}

/* insert a buffer index at the head of the LRU list */
static void insert_buffer_lru( struct native_win_data *win, int index )
{
    unsigned int i;

    for (i = 0; i < NB_CACHED_BUFFERS; i++)
    {
        if (win->buffer_lru[i] == index) break;
        if (win->buffer_lru[i] == -1) break;
    }

    assert( i < NB_CACHED_BUFFERS );
    memmove( win->buffer_lru + 1, win->buffer_lru, i * sizeof(win->buffer_lru[0]) );
    win->buffer_lru[0] = index;
}

static int register_buffer( struct native_win_data *win, struct native_buffer *buffer )
{
    unsigned int i;

    for (i = 0; i < NB_CACHED_BUFFERS; i++)
    {
        if (win->buffers[i] == buffer) goto done;
        if (!win->buffers[i]) break;
    }

    if (i == NB_CACHED_BUFFERS)
    {
        /* reuse the least recently used buffer */
        i = win->buffer_lru[NB_CACHED_BUFFERS - 1];
        assert( i < NB_CACHED_BUFFERS );

        TRACE( "%p %p evicting buffer %p id %d from cache\n",
               win->hwnd, win->win, win->buffers[i], i );
        OH_NativeWindow_NativeObjectUnreference(win->buffers[i]->win_buffer);
        win->fence_id[i] = -1;
    }

    win->buffers[i] = buffer;
    win->fence_id[i] = buffer->fence_id;
    OH_NativeWindow_NativeObjectReference(buffer->win_buffer);
    TRACE( "%p %p %p -> %d\n", win->hwnd, win->win, buffer, i );

done:
    insert_buffer_lru( win, i );
    return i;
}

static struct native_buffer *get_registered_buffer( struct native_win_data *win, int id )
{
    if (id < 0 || id >= NB_CACHED_BUFFERS || !win->buffers[id])
    {
        ERR( "unknown buffer %d for %p %p\n", id, win->hwnd, win->win );
        return NULL;
    }
    return win->buffers[id];
}

static int dequeueBuffer( HWND hwnd, struct native_buffer **buffer )
{
    ohos_window win;
    struct native_win_data *win_data;
    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;
    if (!(win = win_data->win)) return STATUS_DEVICE_NOT_READY;

    ohos_window_buffer win_buffer;
    int fence = -1, ret;
    ret = OH_NativeWindow_NativeWindowRequestBuffer(win, &win_buffer, &fence);
    if (ret != 0) {
        ERR("request buffer failed, ret: %d\n", ret);
        return STATUS_BUFFER_OVERFLOW;
    }
    TRACE( "request buffer %p fence %d\n", win_buffer, fence);

    int width_dst = INT32_MAX;
    int height_dst = INT32_MAX;
    ret = OH_NativeWindow_NativeWindowHandleOpt(win, GET_BUFFER_GEOMETRY, &height_dst, &width_dst);
    if (ret != 0) {
        ERR("get buffer geometry fail ret=%d\n", ret);
        return STATUS_BUFFER_OVERFLOW;
    }
    ohos_buffer_handle buffer_handle =
        OH_NativeWindow_GetBufferHandleFromNative(win_buffer);
    if (!buffer_handle) {
        ERR("get buffer_handle failed\n");
        return STATUS_BUFFER_OVERFLOW;
    }
    
    struct native_buffer *buf = calloc( 1, sizeof(*buf) );

    buf->win_buffer = win_buffer;
    buf->buffer_handle = buffer_handle;
    buf->fence_id = fence;
    buf->width  = width_dst;
    buf->height = height_dst;
    buf->stride = buffer_handle->stride / sizeof(DWORD);

    int buffer_id = register_buffer( win_data, buf );
    buf->buffer_id = buffer_id;
    *buffer = buf;
    TRACE( "%08x got buffer %p fence %d buffer_id %d\n", HandleToLong(hwnd), buffer, fence, buffer_id);

    return STATUS_SUCCESS;
}

static int cancelBuffer( HWND hwnd, int buffer_id )
{
    ohos_window win;
    struct native_buffer *buffer;
    struct native_win_data *win_data;
    int ret;

    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;
    if (!(win = win_data->win)) return STATUS_DEVICE_NOT_READY;

    if (!(buffer = get_registered_buffer( win_data, buffer_id ))) return STATUS_INVALID_HANDLE;

    TRACE( "%08x buffer %p\n", HandleToLong(hwnd), buffer );
    ret = OH_NativeWindow_NativeObjectUnreference(buffer->win_buffer);
    if (ret != 0) {
        ERR("Unreference buffer error: %d", ret);
    }
    return STATUS_SUCCESS;
}

static int queueBuffer( HWND hwnd, struct native_buffer *locked_buffer )
{
    ohos_window win;
    struct native_buffer *buffer;
    struct native_win_data *win_data;
    int ret;

    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;
    if (!(win = win_data->win)) return STATUS_DEVICE_NOT_READY;

    if (!(buffer = get_registered_buffer( win_data, locked_buffer->buffer_id ))) return STATUS_INVALID_HANDLE;
    if (buffer != locked_buffer) return STATUS_INVALID_HANDLE;

    TRACE( "%08x buffer %p fence %d buffer_id %d\n", HandleToLong(hwnd), buffer, buffer->fence_id, buffer->buffer_id);
    Region region = { NULL, 0 };
    ret = OH_NativeWindow_NativeWindowFlushBuffer(win, buffer->win_buffer, buffer->fence_id, region);
    if (ret != 0) {
        ERR("Flush buffer error: %d", ret);
    }
    return STATUS_SUCCESS;
}

int perform( HWND hwnd, int operation, ... )
{
    static const char * const names[] =
    {
        "SET_BUFFER_GEOMETRY", "GET_BUFFER_GEOMETRY",
        "GET_FORMAT", "SET_FORMAT",
        "SET_SWAP_INTERVAL", "GET_SWAP_INTERVAL",
        "NATIVE_WINDOW_LOCK", "NATIVE_WINDOW_UNLOCK_AND_POST",
    };

    int ret = STATUS_SUCCESS;
    ohos_window win;
    struct native_win_data *win_data;
    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;
    if (!(win = win_data->win)) return STATUS_DEVICE_NOT_READY;

    va_list args;

    int args_list[4];
    memset( args_list, 0, sizeof(args_list) );
    va_start( args, operation );
    switch (operation)
    {
    case SET_FORMAT:
        args_list[0] = va_arg( args, int );
        TRACE( "hwnd %p %s arg %d\n", hwnd, names[operation], args_list[0] );
        ret = OH_NativeWindow_NativeWindowHandleOpt(win, operation, args_list[0]);
        if (ret != 0) {
            ERR("set buffer format fail, ret=%d\n", ret);
        }
        if (!ret) win_data->buffer_format = args_list[0];
        break;
    case SET_SWAP_INTERVAL:
        args_list[0] = va_arg( args, int );
        TRACE( "hwnd %p %s arg %d\n", hwnd, names[operation], args_list[0] );
        ret = OH_NativeWindow_NativeWindowHandleOpt(win, operation, args_list[0]);
        if (ret != 0) {
            ERR("set swap interval fail, ret=%d\n", ret);
        }
        if (!ret) win_data->swap_interval = args_list[0];
        break;
    case SET_BUFFER_GEOMETRY:
    {
        args_list[0] = va_arg( args, int );
        args_list[1] = va_arg( args, int );
        TRACE( "hwnd %p %s arg %d-%d\n", hwnd, names[operation], args_list[0], args_list[1] );
        ret = OH_NativeWindow_NativeWindowHandleOpt(win, operation, args_list[0], args_list[1]);
        if (ret != 0) {
            ERR("set buffer geometry fail, ret=%d\n", ret);
        }
    }
    case GET_BUFFER_GEOMETRY:
    {
        RECT *bounds = va_arg( args, RECT * );
        TRACE( "hwnd %p %s\n", hwnd, names[operation]);
        int width_dst = INT32_MAX;
        int height_dst = INT32_MAX;
        ret = OH_NativeWindow_NativeWindowHandleOpt(win, operation, &height_dst, &width_dst);
        if (ret != 0) {
            ERR("get buffer geometry fail, ret=%d\n", ret);
        }
        if (!ret) {
            if (bounds)
            {
                bounds->left   = 0;
                bounds->top    = 0;
                bounds->right  = width_dst;
                bounds->bottom = height_dst;
            }
        }
        break;
    }
    case NATIVE_WINDOW_LOCK:
    {
        struct native_buffer *buffer;
        struct native_buffer *buffer_ret = va_arg( args, struct native_buffer * );
        RECT *bounds = va_arg( args, RECT * );
        ret = dequeueBuffer( hwnd, &buffer );
        if (ret != 0) {
            ERR("dequeueBuffer fail, ret=%d\n", ret);
            break;
        }
        win_data->locked_buffer = buffer;
        if (bounds)
        {
            bounds->left   = 0;
            bounds->top    = 0;
            bounds->right  = buffer->width;
            bounds->bottom = buffer->height;
        }
        *buffer_ret = *buffer;
        TRACE( "hwnd %p %s bounds %s, ret %d\n", hwnd, names[operation], wine_dbgstr_rect(bounds), ret );
        break;
    }
    case NATIVE_WINDOW_UNLOCK_AND_POST:
    {
        TRACE( "hwnd %p %s\n", hwnd, names[operation] );
        ret = STATUS_BUFFER_OVERFLOW;
        if (win_data->locked_buffer)
        {
            ret = queueBuffer( hwnd, win_data->locked_buffer );
            win_data->locked_buffer = NULL;
        }
        TRACE( "hwnd %p %s, ret %d\n", hwnd, names[operation], ret );
        break;
    }
    default:
        FIXME( "unsupported perform hwnd %p op %d %s\n", hwnd, operation,
               operation < ARRAY_SIZE( names ) ? names[operation] : "???" );
        break;
    }
    va_end( args );
    return ret;
}

int set_swap_interval( HWND hwnd, int interval )
{
    ohos_window win;
    struct native_win_data *win_data;
    
    TRACE( "hwnd %08x interval %d\n", HandleToLong(hwnd), interval );

    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;
    win_data->swap_interval = interval;

    if (!(win = win_data->win)) return STATUS_SUCCESS;
    int ret = OH_NativeWindow_NativeWindowHandleOpt(win, SET_SWAP_INTERVAL, interval);
    if (ret != 0) {
        ERR("setSwapInterval_ioctl error: %d\n", ret);
    }
    return STATUS_SUCCESS;
}

void mouse_event_callback( const input_event* event )
{
    static float last_mouse_screen_x = 0;
    static float last_mouse_screen_y = 0;
    union event_data data;

    data.type = MOTION_EVENT;
    data.motion.hwnd = get_hwnd_by_widget_id(event->widget_id);
    data.motion.input.type = INPUT_MOUSE;
    data.motion.input.mi.dx = event->data.mouse_event.screen_x;
    data.motion.input.mi.dy = event->data.mouse_event.screen_y;
    data.motion.input.mi.mouseData   = 0;
    data.motion.input.mi.time        = 0;
    data.motion.input.mi.dwExtraInfo = 0;
    data.motion.input.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    int action = event->data.mouse_event.action;
    int button = event->data.mouse_event.button;
    switch (action) {
        case OH_NATIVEXCOMPONENT_MOUSE_PRESS:
            if (button & OH_NATIVEXCOMPONENT_LEFT_BUTTON)
                data.motion.input.mi.dwFlags |= MOUSEEVENTF_LEFTDOWN;
            if (button & OH_NATIVEXCOMPONENT_RIGHT_BUTTON)
                data.motion.input.mi.dwFlags |= MOUSEEVENTF_RIGHTDOWN;
            if (button & OH_NATIVEXCOMPONENT_MIDDLE_BUTTON)
                data.motion.input.mi.dwFlags |= MOUSEEVENTF_MIDDLEDOWN;
            if ((button & OH_NATIVEXCOMPONENT_BACK_BUTTON) || (button & OH_NATIVEXCOMPONENT_FORWARD_BUTTON))
                return;
            break;
        case OH_NATIVEXCOMPONENT_MOUSE_RELEASE:
        case OH_NATIVEXCOMPONENT_MOUSE_CANCEL:
            if (button & OH_NATIVEXCOMPONENT_LEFT_BUTTON)
                data.motion.input.mi.dwFlags |= MOUSEEVENTF_LEFTUP;
            if (button & OH_NATIVEXCOMPONENT_RIGHT_BUTTON)
                data.motion.input.mi.dwFlags |= MOUSEEVENTF_RIGHTUP;
            if (button & OH_NATIVEXCOMPONENT_MIDDLE_BUTTON)
                data.motion.input.mi.dwFlags |= MOUSEEVENTF_MIDDLEUP;
            if ((button & OH_NATIVEXCOMPONENT_BACK_BUTTON) || (button & OH_NATIVEXCOMPONENT_FORWARD_BUTTON))
                return;
            break;
        case OHOS_MOUSE_ACTION_SCROLL:
            data.motion.input.mi.dx = last_mouse_screen_x;
            data.motion.input.mi.dy = last_mouse_screen_y;
            float wheel_x = event->data.mouse_event.wheel_x;
            float wheel_y = event->data.mouse_event.wheel_y;
            if (wheel_y != 0) {
                data.motion.input.mi.mouseData = (DWORD)(wheel_y * 6.0f);
                data.motion.input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_WHEEL;
                send_event(&data);
            }
            if (wheel_x != 0) {
                data.motion.input.mi.mouseData = (DWORD)(wheel_x * 6.0f);
                data.motion.input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_HWHEEL;
                send_event(&data);
            }
            return;
        case OH_NATIVEXCOMPONENT_MOUSE_MOVE:
            break;
        default:
            return;
    }

    last_mouse_screen_x = data.motion.input.mi.dx;
    last_mouse_screen_y = data.motion.input.mi.dy;

    send_event(&data);
    return;
}

void key_event_callback( const input_event* event )
{
    union event_data data;
    UINT vkey = keycode_to_vkey(event->data.key_event.code);
    if (!vkey) return;

    data.type = KEYBOARD_EVENT;
    data.kbd.hwnd = get_hwnd_by_widget_id(event->widget_id);

    data.kbd.lock_state = 0;
    if (event->data.key_event.is_num_lockon) data.kbd.lock_state |= KEYBOARD_NUM_LOCK;
    if (event->data.key_event.is_caps_lockon) data.kbd.lock_state |= KEYBOARD_CAPS_LOCK;
    if (event->data.key_event.is_scroll_lockon) data.kbd.lock_state |= KEYBOARD_SCROLL_LOCK;

    data.kbd.input.type           = INPUT_KEYBOARD;
    data.kbd.input.ki.wVk         = vkey;
    data.kbd.input.ki.wScan       = vkey_to_scancode[data.kbd.input.ki.wVk];
    data.kbd.input.ki.time        = 0;
    data.kbd.input.ki.dwExtraInfo = 0;
    data.kbd.input.ki.dwFlags     = (data.kbd.input.ki.wScan & 0x100) ? KEYEVENTF_EXTENDEDKEY : 0;

    if (event->data.key_event.action == OH_NATIVEXCOMPONENT_KEY_ACTION_UP )
        data.kbd.input.ki.dwFlags |= KEYEVENTF_KEYUP;

    send_event( &data );
    return;
}

void distribute_input_event_callback( const input_event* event )
{
    switch (event->type) {
        case OHOS_MOUSE_EVENT:
            mouse_event_callback(event);
            break;
        case OHOS_KEY_EVENT:
            key_event_callback(event);
            break;

        case OHOS_WINDOW_RECT_CHANGE_EVENT:
        {
            HWND hwnd = get_hwnd_by_widget_id(event->widget_id);
            if (!hwnd) {
                return;
            }

            ohos_window window = (ohos_window)event->data.window_rect_change_event.window;
            unsigned int width = event->data.window_rect_change_event.width;
            unsigned int height = event->data.window_rect_change_event.height;

            /* Send SURFACE_CHANGED event to event queue */
            union event_data data;
            data.surface.type = SURFACE_CHANGED;
            data.surface.hwnd = hwnd;
            data.surface.window = window;
            data.surface.client = TRUE;
            data.surface.width = width;
            data.surface.height = height;
            send_event(&data);
            break;
        }
        default:
            return;
    }
}

static ohos_window create_window( HWND hwnd, int window_type, const struct window_bounds *bounds )
{
    int ret;
    ohos_window window;
    struct window_param param;
    struct native_win_data *win_data;
    if (!(win_data = create_native_win_data( hwnd )))
        return NULL;

    TRACE( "hwnd %p window_type (%d) window bounds (%d,%d)-(%d,%d)\n", win_data->hwnd, window_type,
        bounds->left, bounds->top, bounds->width, bounds->height );

    param.container = ENTRY_ABILITY;
    param.type = window_type;
    param.hwnd = HandleToLong(hwnd);
    param.pid = getpid();
    param.bounds = *bounds;
    
    if (window_type == MAIN_WINDOW) 
        param.features = get_window_features(hwnd);

    if (window_type == POPUP_WINDOW)
        param.parent_id = get_owner_window_id(hwnd);

    window = CreateNativeWindow(&param, &win_data->widget_id);
    if (!window) {
        ERR("create native window error\n");
        goto failed;
    }
    win_data->win = window;
    win_data->container = param.container;
    win_data->win_type = param.type;
    win_data->generation++;
    ret = OH_NativeWindow_NativeWindowHandleOpt(win_data->win, SET_FORMAT, win_data->buffer_format);
    if (ret != 0) {
        ERR("set format error: %d\n", ret);
        goto failed;
    }
    ret = OH_NativeWindow_NativeWindowHandleOpt(win_data->win, SET_SWAP_INTERVAL, win_data->swap_interval);
    if (ret != 0) {
        ERR("setSwapInterval error: %d\n", ret);
        goto failed;
    }

    TRACE( "hwnd %p widget_id %d\n", win_data->hwnd, win_data->widget_id );
    return window;

failed:
    if (win_data) free_native_win_data(win_data);
    return NULL;
}

ohos_window create_client_window( HWND hwnd )
{
    struct ohos_win_data *data = get_win_data( hwnd );
    int x, y, cx, cy;
    RECT client_rect;

    if (!data)
    {
        /* explicitly create data for HWND_MESSAGE windows since they can be used for OpenGL */
        HWND parent = NtUserGetAncestor( hwnd, GA_PARENT );
        if (parent == NtUserGetDesktopWindow() || NtUserGetAncestor( parent, GA_PARENT )) return 0;
        if (!(data = alloc_win_data( hwnd ))) return 0;
        NtUserGetClientRect( hwnd, &data->rects.client, NtUserGetWinMonitorDpi( hwnd, MDT_RAW_DPI ) );
        data->rects.window = data->rects.visible = data->rects.client;
    }

    x = data->rects.client.left - data->rects.visible.left;
    y = data->rects.client.top - data->rects.visible.top;

    NtUserGetClientRect( hwnd, &client_rect, NtUserGetDpiForWindow( hwnd ) );
    cx = min( max( 1, client_rect.right - client_rect.left ), 65535 );
    cy = min( max( 1, client_rect.bottom - client_rect.top ), 65535 );

    struct window_bounds bounds = { 0 };
    bounds.left = x;
    bounds.top = y;
    bounds.width = cx;
    bounds.height = cy;
    data->window = create_window(hwnd, MAIN_WINDOW, &bounds);

    release_win_data( data );
    return data->window;
}

void create_native_window( struct ohos_win_data *data )
{
    DWORD style, ex_style;
    WCHAR text[1024];
    int window_type;

    TRACE( "hwnd %p window_rects %s\n", data->hwnd, debugstr_window_rects(&data->rects) );

    if (IsRectEmpty(&data->rects.window)) {
        data->lazy_created = TRUE;
        return;
    }

    if (data->rects.visible.right - data->rects.visible.left <= 1 ||
        data->rects.visible.bottom - data->rects.visible.top <= 1) {
        data->lazy_created = TRUE;
        return;
    }

    style = NtUserGetWindowLongW(data->hwnd, GWL_STYLE);
    ex_style = NtUserGetWindowLongW(data->hwnd, GWL_EXSTYLE);

    if (((style & WS_POPUP) || (ex_style & WS_EX_DLGMODALFRAME)))
        window_type = POPUP_WINDOW;
    else
        window_type = MAIN_WINDOW;

    int cx, cy;
    if (!(cx = data->rects.visible.right - data->rects.visible.left)) cx = 1;
    else if (cx > 65535) cx = 800;
    if (!(cy = data->rects.visible.bottom - data->rects.visible.top)) cy = 1;
    else if (cy > 65535) cy = 600;

    struct window_bounds bounds = { 0 };
    bounds.left = data->rects.visible.left;
    bounds.top = data->rects.visible.top;
    bounds.width = cx;
    bounds.height = cy;

    TRACE( "hwnd %p style %08x ex_style %08x bounds (%d,%d)-(%d,%d)\n",
        data->hwnd, style, ex_style, bounds.left, bounds.top, bounds.width, bounds.height );

    data->window = create_window(data->hwnd, window_type, &bounds);
    if (!data->window) {
        return;
    }

    /* set the window text */
    if (!NtUserInternalGetWindowText(data->hwnd, text, ARRAY_SIZE(text))) text[0] = 0;
    set_window_title(data->hwnd, text);
}

void destroy_native_window( HWND hwnd )
{
    struct native_win_data *win_data;
    if (!(win_data = get_native_win_data( hwnd )))
        return;

    TRACE( "hwnd %08x widget_id %d\n", HandleToLong(hwnd), win_data->widget_id );

    CloseNativeWindow(win_data->widget_id);

    if (win_data) free_native_win_data( win_data );
}

int window_pos_changed( HWND hwnd, const struct window_rects *rects,
                        const struct window_rects *old_rects,
                        UINT style, UINT flags )
{
    struct native_win_data *win_data;
    if (!(win_data = get_native_win_data( hwnd )))
        return STATUS_INVALID_PARAMETER;

    TRACE( "hwnd %08x win_rects %s old_rects %s style %08x flags %08x\n",
           HandleToLong( hwnd ), debugstr_window_rects(rects), debugstr_window_rects(old_rects), style, flags );

    if (EqualRect(&rects->visible, &old_rects->visible) ||
          IsRectEmpty( &rects->window ))
        return STATUS_SUCCESS;

    if (HAS_FLAGS(flags, SWP_NOSIZE | SWP_NOMOVE)) {
        TRACE( "window flags contains both SWP_NOSIZE and SWP_NOMOVE\n");
        return STATUS_SUCCESS;
    }

    struct window_bounds bounds = { rects->window.left,
                                    rects->window.top,
                                    rects->window.right - rects->window.left,
                                    rects->window.bottom - rects->window.top };
    SetWindowBounds(win_data->widget_id, &bounds);

    TRACE( "hwnd %08x widget_id %d\n", HandleToLong(hwnd), win_data->widget_id );
    return STATUS_SUCCESS;
}

int set_capture( HWND hwnd )
{
    if (hwnd && !get_native_win_data( hwnd )) return STATUS_INVALID_HANDLE;

    TRACE( "hwnd %08x\n", HandleToLong(hwnd) );

    InterlockedExchangePointer( (void **)&capture_window, hwnd);
    return STATUS_SUCCESS;
}

int set_cursor( HWND hwnd, int id, int width, int height,
                int hotspotx, int hotspoty, const unsigned int *bits )
{
    struct native_win_data *data = get_native_win_data( hwnd );
    if (!data) return STATUS_INVALID_HANDLE;

    if (!bits)
        return STATUS_INVALID_PARAMETER;

    if (width < 0 || height < 0 || width > 256 || height > 256)
        return STATUS_INVALID_PARAMETER;

    TRACE( "hwnd %08x id %d\n", HandleToLong(hwnd), id );
    
    SetCustomCursor(data->widget_id, width, height, hotspotx, hotspoty, bits);
    return STATUS_SUCCESS;
}

int set_window_features( HWND hwnd, const struct window_features* wf)
{
    struct native_win_data *data = get_native_win_data( hwnd );
    if (!data) return STATUS_INVALID_HANDLE;

    if (!wf) return STATUS_INVALID_PARAMETER;

    SetWindowFeatures(data->widget_id, wf);
    return STATUS_SUCCESS;
}

int set_window_state( HWND hwnd, const struct window_state* state)
{
    struct native_win_data *data = get_native_win_data( hwnd );
    if (!data) return STATUS_INVALID_HANDLE;

    if (!state) return STATUS_INVALID_PARAMETER;

    SetWindowState(data->widget_id, state);
    return STATUS_SUCCESS;
}

int set_window_visible( HWND hwnd, int visible)
{
    struct native_win_data *data = get_native_win_data( hwnd );
    if (!data) return STATUS_INVALID_HANDLE;

    SetWindowVisible(data->widget_id, visible);
    return STATUS_SUCCESS;    
}

void set_window_title( HWND hwnd, const WCHAR *text)
{
    DWORD count, text_len;
    char *utf8_buffer;

    struct native_win_data *data = get_native_win_data( hwnd );
    if (!data) return;

    /* allocate new buffer for window text */
    text_len = (lstrlenW(text) + 1) * sizeof(WCHAR);

    RtlUnicodeToUTF8N( NULL, 0, &count, text, text_len );
    if (!(utf8_buffer = malloc( count )))
    {
        return;
    }
    RtlUnicodeToUTF8N( utf8_buffer, count, &count, text, text_len );

    SetWindowTitle(data->widget_id, utf8_buffer);

    free( utf8_buffer );
}

void send_screenlock_event( const char* event_type )
{
    union event_data data;
    int unlocked;

    if (strcmp(event_type, "locked") == 0)
        unlocked = 0;
    else if (strcmp(event_type, "unlocked") == 0)
        unlocked = 1;
    else
        return;
    
    data.screenlock.type = SCREENLOCK_EVENT;
    data.screenlock.unlocked = unlocked;
    send_event(&data);
}

void distribute_screenlock_callback( int unlocked )
{
    TRACE("distribute_screenlock_callback: unlocked=%d\n", unlocked);
    
    if (unlocked)
        ime_on_screen_unlock();
}
