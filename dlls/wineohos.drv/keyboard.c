#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <ace/xcomponent/native_xcomponent_key_event.h>
#include "ohos.h"

#define ARKUI_MODIFIER_KEY_CTRL  (1 << 0)
#define ARKUI_MODIFIER_KEY_SHIFT (1 << 1)
#define ARKUI_MODIFIER_KEY_ALT   (1 << 2)
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ohos);

WORD vkey_to_scancode[] =
{
    0,     /* 0x00 undefined */
    0,     /* VK_LBUTTON */
    0,     /* VK_RBUTTON */
    0,     /* VK_CANCEL */
    0,     /* VK_MBUTTON */
    0,     /* VK_XBUTTON1 */
    0,     /* VK_XBUTTON2 */
    0,     /* 0x07 undefined */
    0x0e,  /* VK_BACK */
    0x0f,  /* VK_TAB */
    0,     /* 0x0a undefined */
    0,     /* 0x0b undefined */
    0,     /* VK_CLEAR */
    0x1c,  /* VK_RETURN */
    0,     /* 0x0e undefined */
    0,     /* 0x0f undefined */
    0x2a,  /* VK_SHIFT */
    0x1d,  /* VK_CONTROL */
    0x38,  /* VK_MENU */
    0,     /* VK_PAUSE */
    0x3a,  /* VK_CAPITAL */
    0,     /* VK_KANA */
    0,     /* 0x16 undefined */
    0,     /* VK_JUNJA */
    0,     /* VK_FINAL */
    0,     /* VK_HANJA */
    0,     /* 0x1a undefined */
    0x01,  /* VK_ESCAPE */
    0,     /* VK_CONVERT */
    0,     /* VK_NONCONVERT */
    0,     /* VK_ACCEPT */
    0,     /* VK_MODECHANGE */
    0x39,  /* VK_SPACE */
    0x149, /* VK_PRIOR */
    0x151, /* VK_NEXT */
    0x14f, /* VK_END */
    0x147, /* VK_HOME */
    0x14b, /* VK_LEFT */
    0x148, /* VK_UP */
    0x14d, /* VK_RIGHT */
    0x150, /* VK_DOWN */
    0,     /* VK_SELECT */
    0,     /* VK_PRINT */
    0,     /* VK_EXECUTE */
    0,     /* VK_SNAPSHOT */
    0x152, /* VK_INSERT */
    0x153, /* VK_DELETE */
    0,     /* VK_HELP */
    0x0b,  /* VK_0 */
    0x02,  /* VK_1 */
    0x03,  /* VK_2 */
    0x04,  /* VK_3 */
    0x05,  /* VK_4 */
    0x06,  /* VK_5 */
    0x07,  /* VK_6 */
    0x08,  /* VK_7 */
    0x09,  /* VK_8 */
    0x0a,  /* VK_9 */
    0,     /* 0x3a undefined */
    0,     /* 0x3b undefined */
    0,     /* 0x3c undefined */
    0,     /* 0x3d undefined */
    0,     /* 0x3e undefined */
    0,     /* 0x3f undefined */
    0,     /* 0x40 undefined */
    0x1e,  /* VK_A */
    0x30,  /* VK_B */
    0x2e,  /* VK_C */
    0x20,  /* VK_D */
    0x12,  /* VK_E */
    0x21,  /* VK_F */
    0x22,  /* VK_G */
    0x23,  /* VK_H */
    0x17,  /* VK_I */
    0x24,  /* VK_J */
    0x25,  /* VK_K */
    0x26,  /* VK_L */
    0x32,  /* VK_M */
    0x31,  /* VK_N */
    0x18,  /* VK_O */
    0x19,  /* VK_P */
    0x10,  /* VK_Q */
    0x13,  /* VK_R */
    0x1f,  /* VK_S */
    0x14,  /* VK_T */
    0x16,  /* VK_U */
    0x2f,  /* VK_V */
    0x11,  /* VK_W */
    0x2d,  /* VK_X */
    0x15,  /* VK_Y */
    0x2c,  /* VK_Z */
    0,     /* VK_LWIN */
    0,     /* VK_RWIN */
    0,     /* VK_APPS */
    0,     /* 0x5e undefined */
    0,     /* VK_SLEEP */
    0x52,  /* VK_NUMPAD0 */
    0x4f,  /* VK_NUMPAD1 */
    0x50,  /* VK_NUMPAD2 */
    0x51,  /* VK_NUMPAD3 */
    0x4b,  /* VK_NUMPAD4 */
    0x4c,  /* VK_NUMPAD5 */
    0x4d,  /* VK_NUMPAD6 */
    0x47,  /* VK_NUMPAD7 */
    0x48,  /* VK_NUMPAD8 */
    0x49,  /* VK_NUMPAD9 */
    0x37,  /* VK_MULTIPLY */
    0x4e,  /* VK_ADD */
    0x7e,  /* VK_SEPARATOR */
    0x4a,  /* VK_SUBTRACT */
    0x53,  /* VK_DECIMAL */
    0135,  /* VK_DIVIDE */
    0x3b,  /* VK_F1 */
    0x3c,  /* VK_F2 */
    0x3d,  /* VK_F3 */
    0x3e,  /* VK_F4 */
    0x3f,  /* VK_F5 */
    0x40,  /* VK_F6 */
    0x41,  /* VK_F7 */
    0x42,  /* VK_F8 */
    0x43,  /* VK_F9 */
    0x44,  /* VK_F10 */
    0x57,  /* VK_F11 */
    0x58,  /* VK_F12 */
    0x64,  /* VK_F13 */
    0x65,  /* VK_F14 */
    0x66,  /* VK_F15 */
    0x67,  /* VK_F16 */
    0x68,  /* VK_F17 */
    0x69,  /* VK_F18 */
    0x6a,  /* VK_F19 */
    0x6b,  /* VK_F20 */
    0,     /* VK_F21 */
    0,     /* VK_F22 */
    0,     /* VK_F23 */
    0,     /* VK_F24 */
    0,     /* 0x88 undefined */
    0,     /* 0x89 undefined */
    0,     /* 0x8a undefined */
    0,     /* 0x8b undefined */
    0,     /* 0x8c undefined */
    0,     /* 0x8d undefined */
    0,     /* 0x8e undefined */
    0,     /* 0x8f undefined */
    0,     /* VK_NUMLOCK */
    0,     /* VK_SCROLL */
    0x10d, /* VK_OEM_NEC_EQUAL = VK_OEM_FJ_JISHO (both are 0x92) */
    0,     /* VK_OEM_FJ_MASSHOU */
    0,     /* VK_OEM_FJ_TOUROKU */
    0,     /* VK_OEM_FJ_LOYA */
    0,     /* VK_OEM_FJ_ROYA */
    0,     /* 0x97 undefined */
    0,     /* 0x98 undefined */
    0,     /* 0x99 undefined */
    0,     /* 0x9a undefined */
    0,     /* 0x9b undefined */
    0,     /* 0x9c undefined */
    0,     /* 0x9d undefined */
    0,     /* 0x9e undefined */
    0,     /* 0x9f undefined */
    0x2a,  /* VK_LSHIFT */
    0x36,  /* VK_RSHIFT */
    0x1d,  /* VK_LCONTROL */
    0x11d, /* VK_RCONTROL */
    0x38,  /* VK_LMENU */
    0x138, /* VK_RMENU */
    0,     /* VK_BROWSER_BACK */
    0,     /* VK_BROWSER_FORWARD */
    0,     /* VK_BROWSER_REFRESH */
    0,     /* VK_BROWSER_STOP */
    0,     /* VK_BROWSER_SEARCH */
    0,     /* VK_BROWSER_FAVORITES */
    0,     /* VK_BROWSER_HOME */
    0x100, /* VK_VOLUME_MUTE */
    0x100, /* VK_VOLUME_DOWN */
    0x100, /* VK_VOLUME_UP */
    0,     /* VK_MEDIA_NEXT_TRACK */
    0,     /* VK_MEDIA_PREV_TRACK */
    0,     /* VK_MEDIA_STOP */
    0,     /* VK_MEDIA_PLAY_PAUSE */
    0,     /* VK_LAUNCH_MAIL */
    0,     /* VK_LAUNCH_MEDIA_SELECT */
    0,     /* VK_LAUNCH_APP1 */
    0,     /* VK_LAUNCH_APP2 */
    0,     /* 0xb8 undefined */
    0,     /* 0xb9 undefined */
    0x27,  /* VK_OEM_1 */
    0x0d,  /* VK_OEM_PLUS */
    0x33,  /* VK_OEM_COMMA */
    0x0c,  /* VK_OEM_MINUS */
    0x34,  /* VK_OEM_PERIOD */
    0x35,  /* VK_OEM_2 */
    0x29,  /* VK_OEM_3 */
    0,     /* 0xc1 undefined */
    0,     /* 0xc2 undefined */
    0,     /* 0xc3 undefined */
    0,     /* 0xc4 undefined */
    0,     /* 0xc5 undefined */
    0,     /* 0xc6 undefined */
    0,     /* 0xc7 undefined */
    0,     /* 0xc8 undefined */
    0,     /* 0xc9 undefined */
    0,     /* 0xca undefined */
    0,     /* 0xcb undefined */
    0,     /* 0xcc undefined */
    0,     /* 0xcd undefined */
    0,     /* 0xce undefined */
    0,     /* 0xcf undefined */
    0,     /* 0xd0 undefined */
    0,     /* 0xd1 undefined */
    0,     /* 0xd2 undefined */
    0,     /* 0xd3 undefined */
    0,     /* 0xd4 undefined */
    0,     /* 0xd5 undefined */
    0,     /* 0xd6 undefined */
    0,     /* 0xd7 undefined */
    0,     /* 0xd8 undefined */
    0,     /* 0xd9 undefined */
    0,     /* 0xda undefined */
    0x1a,  /* VK_OEM_4 */
    0x2b,  /* VK_OEM_5 */
    0x1b,  /* VK_OEM_6 */
    0x28,  /* VK_OEM_7 */
    0,     /* VK_OEM_8 */
    0,     /* 0xe0 undefined */
    0,     /* VK_OEM_AX */
    0x56,  /* VK_OEM_102 */
    0,     /* VK_ICO_HELP */
    0,     /* VK_ICO_00 */
    0,     /* VK_PROCESSKEY */
    0,     /* VK_ICO_CLEAR */
    0,     /* VK_PACKET */
    0,     /* 0xe8 undefined */
    0x71,  /* VK_OEM_RESET */
    0,     /* VK_OEM_JUMP */
    0,     /* VK_OEM_PA1 */
    0,     /* VK_OEM_PA2 */
    0,     /* VK_OEM_PA3 */
    0,     /* VK_OEM_WSCTRL */
    0,     /* VK_OEM_CUSEL */
    0,     /* VK_OEM_ATTN */
    0,     /* VK_OEM_FINISH */
    0,     /* VK_OEM_COPY */
    0,     /* VK_OEM_AUTO */
    0,     /* VK_OEM_ENLW */
    0,     /* VK_OEM_BACKTAB */
    0,     /* VK_ATTN */
    0,     /* VK_CRSEL */
    0,     /* VK_EXSEL */
    0,     /* VK_EREOF */
    0,     /* VK_PLAY */
    0,     /* VK_ZOOM */
    0,     /* VK_NONAME */
    0,     /* VK_PA1 */
    0x59,  /* VK_OEM_CLEAR */
    0,     /* 0xff undefined */
};

const UINT keycode_to_vkey(int keycode)
{
    switch (keycode) {
        case KEY_0: return '0';
        case KEY_1: return '1';
        case KEY_2: return '2';
        case KEY_3: return '3';
        case KEY_4: return '4';
        case KEY_5: return '5';
        case KEY_6: return '6';
        case KEY_7: return '7';
        case KEY_8: return '8';
        case KEY_9: return '9';
        case KEY_DPAD_UP: return VK_UP;
        case KEY_DPAD_DOWN: return VK_DOWN;
        case KEY_DPAD_LEFT: return VK_LEFT;
        case KEY_DPAD_RIGHT: return VK_RIGHT;
        case KEY_A: return 'A';
        case KEY_B: return 'B';
        case KEY_C: return 'C';
        case KEY_D: return 'D';
        case KEY_E: return 'E';
        case KEY_F: return 'F';
        case KEY_G: return 'G';
        case KEY_H: return 'H';
        case KEY_I: return 'I';
        case KEY_J: return 'J';
        case KEY_K: return 'K';
        case KEY_L: return 'L';
        case KEY_M: return 'M';
        case KEY_N: return 'N';
        case KEY_O: return 'O';
        case KEY_P: return 'P';
        case KEY_Q: return 'Q';
        case KEY_R: return 'R';
        case KEY_S: return 'S';
        case KEY_T: return 'T';
        case KEY_U: return 'U';
        case KEY_V: return 'V';
        case KEY_W: return 'W';
        case KEY_X: return 'X';
        case KEY_Y: return 'Y';
        case KEY_Z: return 'Z';
        case KEY_COMMA: return VK_OEM_COMMA;
        case KEY_PERIOD: return VK_OEM_PERIOD;
        case KEY_ALT_LEFT: return VK_LMENU;
        case KEY_ALT_RIGHT: return VK_RMENU;
        case KEY_SHIFT_LEFT: return VK_LSHIFT;
        case KEY_SHIFT_RIGHT: return VK_RSHIFT;
        case KEY_TAB: return VK_TAB;
        case KEY_SPACE: return VK_SPACE;
        case KEY_ENTER: return VK_RETURN;
        case KEY_DEL: return VK_BACK;
        case KEY_GRAVE: return VK_OEM_3;
        case KEY_MINUS: return VK_OEM_MINUS;
        case KEY_EQUALS: return VK_OEM_PLUS;
        case KEY_LEFT_BRACKET: return VK_OEM_4;
        case KEY_RIGHT_BRACKET: return VK_OEM_6;
        case KEY_BACKSLASH: return VK_OEM_5;
        case KEY_SEMICOLON: return VK_OEM_1;
        case KEY_APOSTROPHE: return VK_OEM_7;
        case KEY_SLASH: return VK_OEM_2;
        case KEY_MEDIA_PLAY_PAUSE: return VK_MEDIA_PLAY_PAUSE;
        case KEY_MEDIA_STOP: return VK_MEDIA_STOP;
        case KEY_MEDIA_NEXT: return VK_MEDIA_NEXT_TRACK;
        case KEY_MEDIA_PREVIOUS: return VK_MEDIA_PREV_TRACK;
        case KEY_PAGE_UP: return VK_PRIOR;
        case KEY_PAGE_DOWN: return VK_NEXT;
        case KEY_ESCAPE: return VK_ESCAPE;
        case KEY_FORWARD_DEL: return VK_DELETE;
        case KEY_CTRL_LEFT: return VK_LCONTROL;
        case KEY_CTRL_RIGHT: return VK_RCONTROL;
        case KEY_CAPS_LOCK: return VK_CAPITAL;
        case KEY_SCROLL_LOCK: return VK_SCROLL;
        case KEY_META_LEFT: return VK_LWIN;
        case KEY_META_RIGHT: return VK_RWIN;
        case KEY_MOVE_HOME: return VK_HOME;
        case KEY_MOVE_END: return VK_END;
        case KEY_INSERT: return VK_INSERT;
        case KEY_F1: return VK_F1;
        case KEY_F2: return VK_F2;
        case KEY_F3: return VK_F3;
        case KEY_F4: return VK_F4;
        case KEY_F5: return VK_F5;
        case KEY_F6: return VK_F6;
        case KEY_F7: return VK_F7;
        case KEY_F8: return VK_F8;
        case KEY_F9: return VK_F9;
        case KEY_F10: return VK_F10;
        case KEY_F11: return VK_F11;
        case KEY_F12: return VK_F12;
        case KEY_NUM_LOCK: return VK_NUMLOCK;
        case KEY_NUMPAD_0: return VK_NUMPAD0;
        case KEY_NUMPAD_1: return VK_NUMPAD1;
        case KEY_NUMPAD_2: return VK_NUMPAD2;
        case KEY_NUMPAD_3: return VK_NUMPAD3;
        case KEY_NUMPAD_4: return VK_NUMPAD4;
        case KEY_NUMPAD_5: return VK_NUMPAD5;
        case KEY_NUMPAD_6: return VK_NUMPAD6;
        case KEY_NUMPAD_7: return VK_NUMPAD7;
        case KEY_NUMPAD_8: return VK_NUMPAD8;
        case KEY_NUMPAD_9: return VK_NUMPAD9;
        case KEY_NUMPAD_DIVIDE: return VK_DIVIDE;
        case KEY_NUMPAD_MULTIPLY: return VK_MULTIPLY;
        case KEY_NUMPAD_SUBTRACT: return VK_SUBTRACT;
        case KEY_NUMPAD_ADD: return VK_ADD;
        case KEY_NUMPAD_DOT: return VK_DECIMAL;
        case KEY_HENKAN: return VK_KANA;
        default: return 0;
    }
}

void send_keyboard_input( HWND hwnd, WORD vkey, WORD scan, DWORD flags )
{
    INPUT input;

    input.type           = INPUT_KEYBOARD;
    input.ki.wVk         = vkey;
    input.ki.wScan       = scan;
    input.ki.dwFlags     = flags;
    input.ki.time        = 0;
    input.ki.dwExtraInfo = 0;

    NtUserSendHardwareInput( hwnd, 0, &input, 0 );
}

/***********************************************************************
 *           update_keyboard_lock_state
 */
void update_keyboard_lock_state( WORD vkey, UINT state )
{
    BYTE keystate[256];

    if (!NtUserGetAsyncKeyboardState( keystate )) return;

    if (!(keystate[VK_CAPITAL] & 0x01) != !(state & KEYBOARD_CAPS_LOCK) && vkey != VK_CAPITAL)
    {
        TRACE( "adjusting CapsLock state (%02x)\n", keystate[VK_CAPITAL] );
        send_keyboard_input( 0, VK_CAPITAL, 0x3a, 0 );
        send_keyboard_input( 0, VK_CAPITAL, 0x3a, KEYEVENTF_KEYUP );
    }

    if (!(keystate[VK_NUMLOCK] & 0x01) != !(state & KEYBOARD_NUM_LOCK) && (vkey & 0xff) != VK_NUMLOCK)
    {
        TRACE( "adjusting NumLock state (%02x)\n", keystate[VK_NUMLOCK] );
        send_keyboard_input( 0, VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY );
        send_keyboard_input( 0, VK_NUMLOCK, 0x45, KEYEVENTF_EXTENDEDKEY | KEYEVENTF_KEYUP );
    }

    if (!(keystate[VK_SCROLL] & 0x01) != !(state & KEYBOARD_SCROLL_LOCK) && vkey != VK_SCROLL)
    {
        TRACE( "adjusting ScrollLock state (%02x)\n", keystate[VK_SCROLL] );
        send_keyboard_input( 0, VK_SCROLL, 0x46, 0 );
        send_keyboard_input( 0, VK_SCROLL, 0x46, KEYEVENTF_KEYUP );
    }
}

/***********************************************************************
 *           sync_modifier_key_state
 *
 */
void sync_modifier_key_state( WORD vkey, uint64_t modifier_key_states )
{
    BYTE keystate[256];
    BOOL changed = FALSE;

    if (!NtUserGetAsyncKeyboardState( keystate )) return;

    if (vkey != VK_LMENU && vkey != VK_RMENU &&
        (keystate[VK_MENU] & 0x80) && !(modifier_key_states & ARKUI_MODIFIER_KEY_ALT))
    {
        TRACE( "adjusting Alt state (%02x/%02x)\n", keystate[VK_LMENU], keystate[VK_RMENU] );
        keystate[VK_LMENU] &= ~0x80;
        keystate[VK_RMENU] &= ~0x80;
        keystate[VK_MENU]  &= ~0x80;
        changed = TRUE;
    }

    if (vkey != VK_LCONTROL && vkey != VK_RCONTROL &&
        (keystate[VK_CONTROL] & 0x80) && !(modifier_key_states & ARKUI_MODIFIER_KEY_CTRL))
    {
        TRACE( "adjusting Ctrl state (%02x/%02x)\n", keystate[VK_LCONTROL], keystate[VK_RCONTROL] );
        keystate[VK_LCONTROL] &= ~0x80;
        keystate[VK_RCONTROL] &= ~0x80;
        keystate[VK_CONTROL]  &= ~0x80;
        changed = TRUE;
    }

    if (vkey != VK_LSHIFT && vkey != VK_RSHIFT &&
        (keystate[VK_SHIFT] & 0x80) && !(modifier_key_states & ARKUI_MODIFIER_KEY_SHIFT))
    {
        TRACE( "adjusting Shift state (%02x/%02x)\n", keystate[VK_LSHIFT], keystate[VK_RSHIFT] );
        keystate[VK_LSHIFT] &= ~0x80;
        keystate[VK_RSHIFT] &= ~0x80;
        keystate[VK_SHIFT]  &= ~0x80;
        changed = TRUE;
    }

    if (!changed) return;

    SERVER_START_REQ( set_key_state )
    {
        req->async = 1;
        wine_server_add_data( req, keystate, sizeof(keystate) );
        wine_server_call( req );
    }
    SERVER_END_REQ;
}