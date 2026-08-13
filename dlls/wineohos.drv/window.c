#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <assert.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#define OEMRESOURCE
#include "windef.h"
#include "winbase.h"
#include "wingdi.h"
#include "winuser.h"
#include "ntuser.h"

#include "ohos.h"
#include "surface_lut.h"
#include "wine/server.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ohos);

#define SWP_AGG_NOPOSCHANGE (SWP_NOSIZE | SWP_NOMOVE | SWP_NOCLIENTSIZE | SWP_NOCLIENTMOVE | SWP_NOZORDER)

pthread_mutex_t win_data_mutex;

static struct ohos_win_data *win_data_context[32768];

static inline int context_idx( HWND hwnd )
{
    return LOWORD( hwnd ) >> 1;
}

static inline int get_dib_stride( int width, int bpp )
{
    return ((width * bpp + 31) >> 3) & ~3;
}

static inline int get_dib_image_size( const BITMAPINFO *info )
{
    return get_dib_stride( info->bmiHeader.biWidth, info->bmiHeader.biBitCount )
        * abs( info->bmiHeader.biHeight );
}

static BOOL intersect_rect( RECT *dst, const RECT *src1, const RECT *src2 )
{
    dst->left   = max(src1->left, src2->left);
    dst->top    = max(src1->top, src2->top);
    dst->right  = min(src1->right, src2->right);
    dst->bottom = min(src1->bottom, src2->bottom);
    return !IsRectEmpty( dst );
}

/***********************************************************************
 *           alloc_win_data
 */
struct ohos_win_data *alloc_win_data( HWND hwnd )
{
    struct ohos_win_data *data;

    if ((data = calloc( 1, sizeof(*data) )))
    {
        data->hwnd = hwnd;
        pthread_mutex_lock( &win_data_mutex );
        win_data_context[context_idx(hwnd)] = data;
    }
    return data;
}


/***********************************************************************
 *           free_win_data
 */
void free_win_data( struct ohos_win_data *data )
{
    win_data_context[context_idx( data->hwnd )] = NULL;
    pthread_mutex_unlock( &win_data_mutex );
    if (data->widget_id > 0) destroy_native_window( data->hwnd );
    data->widget_id = -1;
    data->on_screen = FALSE;
    free( data );
}


/***********************************************************************
 *           get_win_data
 *
 * Lock and return the data structure associated with a window.
 */
struct ohos_win_data *get_win_data( HWND hwnd )
{
    struct ohos_win_data *data;

    if (!hwnd) return NULL;
    pthread_mutex_lock( &win_data_mutex );
    if ((data = win_data_context[context_idx(hwnd)]) && data->hwnd == hwnd) return data;
    pthread_mutex_unlock( &win_data_mutex );
    return NULL;
}


/***********************************************************************
 *           release_win_data
 *
 * Release the data returned by get_win_data.
 */
void release_win_data( struct ohos_win_data *data )
{
    if (data) pthread_mutex_unlock( &win_data_mutex );
}


/***********************************************************************
 *           get_window
 */
static int get_window( HWND hwnd )
{
    struct ohos_win_data *data = get_win_data( hwnd );

    if (!data || data->widget_id <= 0) return -1;
    release_win_data( data );
    return data->widget_id;
}

static inline BOOL can_window_become_foreground(HWND hwnd)
{
    LONG style = NtUserGetWindowLongW(hwnd, GWL_STYLE);

    if (!(style & WS_VISIBLE)) return FALSE;
    if ((style & (WS_POPUP | WS_CHILD)) == WS_CHILD) return FALSE;
    if (hwnd == NtUserGetDesktopWindow()) return FALSE;
    return !(style & WS_DISABLED);
}

struct window_state get_window_state(HWND hwnd)
{
    struct window_state ws = {0};
    DWORD style, ex_style;

    style = NtUserGetWindowLongW(hwnd, GWL_STYLE);
    ex_style = NtUserGetWindowLongW(hwnd, GWL_EXSTYLE);

    ws.disabled = (style & WS_DISABLED) != 0;
    ws.no_foreground = !can_window_become_foreground(hwnd);
    ws.top_most = (ex_style & WS_EX_TOPMOST) != 0;
    ws.excluded_by_control = (!(ex_style & WS_EX_APPWINDOW) &&
                                (NtUserGetWindowRelative(hwnd, GW_OWNER) ||
                                (ex_style & (WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE))));
    ws.minimized = (style & WS_MINIMIZE) != 0;
    ws.maximized = (style & WS_MAXIMIZE) != 0;

    ws.visible = ((style & WS_VISIBLE) == WS_VISIBLE) && (!(ex_style & WS_EX_LAYERED));
    return ws;
}

struct window_features get_window_features_for_style(DWORD style, DWORD ex_style)
{
    struct window_features wf = {0};

    if ((ex_style & WS_EX_NOACTIVATE) || (style & WS_CAPTION) != WS_CAPTION)
    {
        wf.prevents_app_activation = TRUE;
    }

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

    wf.touchable = TRUE;
    wf.use_floating_window = FALSE;
    wf.use_dark_mode = FALSE;
    return wf;
}

static void set_window_properties(struct ohos_win_data *data)
{
    int widget_id;
    DWORD style, ex_style;
    HWND owner;
    struct ohos_win_data *owner_data;
    struct window_features wf;
    struct window_state state;

    widget_id = get_widget_id(data->hwnd);
    style = NtUserGetWindowLongW(data->hwnd, GWL_STYLE);
    ex_style = NtUserGetWindowLongW(data->hwnd, GWL_EXSTYLE);

    owner = NtUserGetWindowRelative(data->hwnd, GW_OWNER);
    if (owner)
        owner = NtUserGetAncestor(owner, GA_ROOT);
    owner_data = get_win_data( owner );
    if (owner_data) SetParentWindow(widget_id, get_widget_id(owner_data->hwnd));

    wf = get_window_features_for_style(style, ex_style);
    SetWindowFeatures(widget_id, &wf);

    state = get_window_state(data->hwnd);
    SetWindowState(widget_id, &state);

    if (state.minimized != data->minimized)
        data->minimized = state.minimized;

    release_win_data( owner_data );
}

static void sync_window_opacity(struct ohos_win_data *data, BYTE alpha,
                                BOOL per_pixel_alpha, DWORD flags)
{
    float opacity = 1.0;

    if (flags & LWA_ALPHA) opacity = alpha / 255.0;

    TRACE("setting window %p/%d alpha to %g\n", data->hwnd, data->widget_id, opacity);
    SetWindowAlpha(get_widget_id(data->hwnd), opacity);
}

static BOOL ohos_window_size_change_notify( HWND hwnd, const RECT *rect, int reason )
{
    struct ohos_win_data *data;

    if (!hwnd) return FALSE;
    if (!(data = get_win_data( hwnd ))) return FALSE;

    TRACE( "hwnd %p rect %s reason %d\n", hwnd, wine_dbgstr_rect(rect), reason );

    /* When Wine has initiated a position/size change (pending_configure),
     * ignore OS callbacks whose rect doesn't match the expected pending_rect.
     * The OHOS compositor may send stale cached positions (reason=0) for
     * sub-windows after destroy->create reuse, or generate self-correction
     * callbacks with wrong positions after multiple rapid SetWindowBounds.
     * Only callbacks matching pending_rect are accepted; others are dropped
     * to prevent Wine from adopting incorrect positions. */
    if (data->pending_configure && !EqualRect( rect, &data->pending_rect ))
    {
        TRACE( "hwnd %p: ignoring stale OS rect %s while pending %s\n",
               hwnd, wine_dbgstr_rect(rect), wine_dbgstr_rect(&data->pending_rect) );
        release_win_data( data );
        return FALSE;
    }

    data->os_pending_rect = *rect;
    data->os_pending_reason = reason;
    data->os_rect_pending = TRUE;

    release_win_data( data );

    return NtUserPostMessage( hwnd, WM_WINE_WINDOW_STATE_CHANGED, 0, 0 );
}

/***********************************************************************
 *           OHOS_GetWindowStateUpdates
 */
BOOL OHOS_GetWindowStateUpdates( HWND hwnd, UINT *state_cmd, UINT *config_cmd,
                                  RECT *rect, HWND *foreground )
{
    struct ohos_win_data *data;

    *state_cmd = *config_cmd = 0;
    *foreground = 0;

    if (!(data = get_win_data( hwnd ))) return FALSE;

    if (!data->os_rect_pending)
    {
        release_win_data( data );
        return FALSE;
    }
    data->os_rect_pending = FALSE;

    RECT old_rect = data->rects.window;
    *rect = data->os_pending_rect;
    int reason = data->os_pending_reason;

    if (data->pending_configure)
    {
        data->pending_configure = FALSE;
        data->pending_configure_time = 0;
        if (EqualRect( rect, &data->pending_rect ))
        {
            TRACE( "hwnd %p: confirmation for Wine-initiated change, skip\n", hwnd );
            release_win_data( data );
            return FALSE;
        }
        /* The OS reported a rect different from what Wine set while
         * pending_configure was still TRUE.  This means the compositor
         * sent a stale/wrong position.  Reject it. */
        TRACE( "hwnd %p: OS rect %s differs from pending %s, rejecting\n",
               hwnd, wine_dbgstr_rect(rect), wine_dbgstr_rect(&data->pending_rect) );
        release_win_data( data );
        return FALSE;
    }

    release_win_data( data );

    UINT flags = SWP_NOACTIVATE | SWP_NOZORDER;
    if (old_rect.left == rect->left && old_rect.top == rect->top)
        flags |= SWP_NOMOVE;
    if ((old_rect.right - old_rect.left == rect->right - rect->left) &&
        (old_rect.bottom - old_rect.top == rect->bottom - rect->top))
        flags |= SWP_NOSIZE;

    if ((flags & (SWP_NOSIZE | SWP_NOMOVE)) == (SWP_NOSIZE | SWP_NOMOVE))
        return FALSE;

    flags |= SWP_NOSENDCHANGING;
    *config_cmd = MAKELONG( SC_MOVE, flags );

    if (reason == 2) /* RECOVER */
        *state_cmd = SC_RESTORE;
    else if (old_rect.left <= -32000 && old_rect.top <= -32000 &&
             rect->left > -32000 && rect->top > -32000)
        *state_cmd = SC_RESTORE;

    TRACE( "hwnd %p, config_cmd %#x, state_cmd %#x, rect %s, reason %d\n",
           hwnd, *config_cmd, *state_cmd, wine_dbgstr_rect(rect), reason );
    return TRUE;
}

/* Handling of events coming from the arkui side */

struct ui_event
{
    struct list      entry;
    union event_data data;
};

static struct list event_queue = LIST_INIT( event_queue );
static struct ui_event *current_event;
static int event_pipe[2];
static DWORD desktop_tid;

/***********************************************************************
 *           send_event
 */
int send_event( const union event_data *data )
{
    int res;

    if ((res = write( event_pipe[1], data, sizeof(*data) )) != sizeof(*data))
    {
        return -1;
    }
    return 0;
}

/***********************************************************************
 *           init_event_queue
 */
static void init_event_queue(void)
{
    HANDLE handle;
    int ret;

    if (pipe2( event_pipe, O_CLOEXEC | O_NONBLOCK ) == -1)
    {
        ERR( "could not create data\n" );
        NtTerminateProcess( 0, 1 );
    }
    if (wine_server_fd_to_handle( event_pipe[0], GENERIC_READ | SYNCHRONIZE, 0, &handle ))
    {
        ERR( "Can't allocate handle for event fd\n" );
        NtTerminateProcess( 0, 1 );
    }
    SERVER_START_REQ( set_queue_fd )
    {
        req->handle = wine_server_obj_handle( handle );
        ret = wine_server_call( req );
    }
    SERVER_END_REQ;
    if (ret)
    {
        ERR( "Can't store handle for event fd %x\n", ret );
        NtTerminateProcess( 0, 1 );
    }
    NtClose( handle );
    desktop_tid = GetCurrentThreadId();
    TRACE( "desktop_tid: %x, event_pipe(%d,%d)\n", desktop_tid, event_pipe[0], event_pipe[1] );
}


/***********************************************************************
 *           pull_events
 *
 * Pull events from the event pipe and add them to the queue
 */
static void pull_events(void)
{
    struct ui_event *event;
    int res;

    for (;;)
    {
        if (!(event = malloc( sizeof(*event) ))) break;

        res = read( event_pipe[0], &event->data, sizeof(event->data) );
        if (res != sizeof(event->data)) break;
        list_add_tail( &event_queue, &event->entry );
    }
    free( event );
}


/***********************************************************************
 *           process_events
 */
static int process_events( DWORD mask )
{
    struct ui_event *event, *next, *previous;
    unsigned int count = 0;

    assert( GetCurrentThreadId() == desktop_tid );

    pull_events();

    previous = current_event;

    LIST_FOR_EACH_ENTRY_SAFE( event, next, &event_queue, struct ui_event, entry )
    {
        switch (event->data.type)
        {
        case SURFACE_CHANGED:
            break;  /* always process it to unblock other threads */
        case MOTION_EVENT:
            if (event->data.motion.input.mi.dwFlags & (MOUSEEVENTF_LEFTDOWN|MOUSEEVENTF_RIGHTDOWN|
                                                       MOUSEEVENTF_MIDDLEDOWN|MOUSEEVENTF_LEFTUP|
                                                       MOUSEEVENTF_RIGHTUP|MOUSEEVENTF_MIDDLEUP))
            {
                if (mask & QS_MOUSEBUTTON) break;
            }
            else if (mask & QS_MOUSEMOVE) break;
            continue;  /* skip it */
        case KEYBOARD_EVENT:
            if (mask & QS_KEY) break;
            continue;  /* skip it */
        case IME_EVENT:
            break;
        case SCREENLOCK_EVENT:
            break;
        case WINDOW_FOCUS_EVENT:
            break;
        default:
            if (mask & QS_SENDMESSAGE) break;
            continue;  /* skip it */
        }

        /* remove it first, in case we process events recursively */
        list_remove( &event->entry );
        current_event = event;

        switch (event->data.type)
        {
        case DESKTOP_CHANGED:
            TRACE( "DESKTOP_CHANGED %ux%u\n", event->data.desktop.width, event->data.desktop.height );
            screen_width = event->data.desktop.width;
            screen_height = event->data.desktop.height;
            init_monitors( screen_width, screen_height );
            break;

        case CONFIG_CHANGED:
            TRACE( "CONFIG_CHANGED dpi %u\n", event->data.cfg.dpi );
            set_screen_dpi( event->data.cfg.dpi );
            break;

        case SURFACE_CHANGED: {
            RECT rect;
            int left = event->data.surface.left;
            int top = event->data.surface.top;
            unsigned int width = event->data.surface.width;
            unsigned int height = event->data.surface.height;
            TRACE("SURFACE_CHANGED %p size %d,%d %ux%u reason %d\n", event->data.surface.hwnd,
                  left, top, width, height, event->data.surface.reason);

            SetRect(&rect, left, top, left + width, top + height);
            ohos_window_size_change_notify(event->data.surface.hwnd, &rect, event->data.surface.reason);
            break;
        }
        case MOTION_EVENT: {
            HWND capture = get_capture_window();

            if (event->data.motion.input.mi.dwFlags &
                (MOUSEEVENTF_LEFTDOWN | MOUSEEVENTF_RIGHTDOWN | MOUSEEVENTF_MIDDLEDOWN))
                TRACE("BUTTONDOWN pos %d,%d hwnd %p flags %x\n", event->data.motion.input.mi.dx,
                      event->data.motion.input.mi.dy, event->data.motion.hwnd, event->data.motion.input.mi.dwFlags);
            else if (event->data.motion.input.mi.dwFlags &
                     (MOUSEEVENTF_LEFTUP | MOUSEEVENTF_RIGHTUP | MOUSEEVENTF_MIDDLEUP))
                TRACE("BUTTONUP pos %d,%d hwnd %p flags %x\n", event->data.motion.input.mi.dx,
                      event->data.motion.input.mi.dy, event->data.motion.hwnd, event->data.motion.input.mi.dwFlags);
            else
                TRACE("MOUSEMOVE pos %d,%d hwnd %p flags %x\n", event->data.motion.input.mi.dx,
                      event->data.motion.input.mi.dy, event->data.motion.hwnd, event->data.motion.input.mi.dwFlags);
            if (!capture && (event->data.motion.input.mi.dwFlags & MOUSEEVENTF_ABSOLUTE)) {
                RECT rect;
                SetRect(&rect, event->data.motion.input.mi.dx, event->data.motion.input.mi.dy,
                        event->data.motion.input.mi.dx + 1, event->data.motion.input.mi.dy + 1);

                SERVER_START_REQ(update_window_zorder)
                {
                    req->window = wine_server_user_handle(event->data.motion.hwnd);
                    req->rect = wine_server_rectangle(rect);
                    wine_server_call(req);
                }
                SERVER_END_REQ;
            }
            TRACE("MOUSEMOVE hwnd %p capture %p\n", event->data.motion.hwnd, capture);
            NtUserSendHardwareInput(capture ? capture : event->data.motion.hwnd, 0, &event->data.motion.input, 0);
        } break;
        case KEYBOARD_EVENT:
            if (event->data.kbd.input.ki.dwFlags & KEYEVENTF_KEYUP)
                TRACE("KEYUP hwnd %p vkey %x '%c' scancode %x\n", event->data.kbd.hwnd, event->data.kbd.input.ki.wVk,
                      event->data.kbd.input.ki.wVk, event->data.kbd.input.ki.wScan);
            else
                TRACE("KEYDOWN hwnd %p vkey %x '%c' scancode %x\n", event->data.kbd.hwnd, event->data.kbd.input.ki.wVk,
                      event->data.kbd.input.ki.wVk, event->data.kbd.input.ki.wScan);
            update_keyboard_lock_state(event->data.kbd.input.ki.wVk, event->data.kbd.lock_state);
            sync_modifier_key_state(event->data.kbd.input.ki.wVk, event->data.kbd.modifier_key_states);
            NtUserSendHardwareInput(0, 0, &event->data.kbd.input, 0);
            break;
        case IME_EVENT:
            distribute_ime_callback(&(event->data.ime.data));
            break;
        case SCREENLOCK_EVENT:
            distribute_screenlock_callback(event->data.screenlock.unlocked);
            break;
        case WINDOW_FOCUS_EVENT: {
            HWND hwnd = event->data.focus.hwnd;
            BOOL gained = event->data.focus.gained;
            GUITHREADINFO info;
            info.cbSize = sizeof(info);
            HWND focused_hwnd = NtUserGetGUIThreadInfo(GetCurrentThreadId(), &info) ? info.hwndFocus : 0;

            TRACE("WINDOW_FOCUS_EVENT: hwnd=%p, gained=%d, current_focus=%p\n", hwnd, gained, focused_hwnd);
            
            if (gained) {
                HWND root = NtUserGetAncestor(hwnd, GA_ROOT);
                if (root && root != focused_hwnd) {
                    NtUserSetFocus(root);
                }
                if (hwnd != root && hwnd != focused_hwnd) {
                    NtUserSetFocus(hwnd);
                }
            }
            break;
        }
        default:
            FIXME("got event %u\n", event->data.type);
        }
        free( event );
        count++;
        /* next may have been removed by a recursive call, so reset it to the beginning of the list */
        next = LIST_ENTRY( event_queue.next, struct ui_event, entry );
    }
    current_event = previous;
    return count;
}


/***********************************************************************
 *           wait_events
 */
static int wait_events( int timeout )
{
    assert( GetCurrentThreadId() == desktop_tid );

    for (;;)
    {
        struct pollfd pollfd;
        int ret;

        pollfd.fd = event_pipe[0];
        pollfd.events = POLLIN | POLLHUP;
        ret = poll( &pollfd, 1, timeout );
        if (ret == -1 && errno == EINTR) continue;
        if (ret && (pollfd.revents & (POLLHUP | POLLERR))) ret = -1;
        return ret;
    }
}


/* Window surface support */

struct ohos_window_surface
{
    struct window_surface header;
    int                   widget_id;
    UINT                  clip_count;
    RECT                  *clip_rects;
};

static struct ohos_window_surface *get_ohos_surface( struct window_surface *surface )
{
    return (struct ohos_window_surface *)surface;
}

/* apply the window region to a single line of the destination image. */
static void apply_line_region( DWORD *dst, int width, int x, int y, const RECT *rect, const RECT *end )
{
    while (rect < end && rect->top <= y && width > 0)
    {
        if (rect->left > x)
        {
            memset( dst, 0, min( rect->left - x, width ) * sizeof(*dst) );
            dst += rect->left - x;
            width -= rect->left - x;
            x = rect->left;
        }
        if (rect->right > x)
        {
            dst += rect->right - x;
            width -= rect->right - x;
            x = rect->right;
        }
        rect++;
    }
    if (width > 0) memset( dst, 0, width * sizeof(*dst) );
}

/***********************************************************************
 *           ohos_surface_set_clip
 */
static void ohos_surface_set_clip( struct window_surface *window_surface, const RECT *rects, UINT count )
{
    struct ohos_window_surface *surface = get_ohos_surface( window_surface );

    TRACE( "surface %p, rects %p, count %u\n", surface, rects, count );

    free( surface->clip_rects );
    surface->clip_rects = NULL;

    if (!count || !(surface->clip_rects = malloc( count * sizeof(*rects) ))) return;
    memcpy( surface->clip_rects, rects, count * sizeof(*rects) );
    surface->clip_count = count;
}

/***********************************************************************
 *           ohos_surface_flush
 */
static BOOL ohos_surface_flush( struct window_surface *window_surface, const RECT *rect, const RECT *dirty,
                                   const BITMAPINFO *color_info, const void *color_bits, BOOL shape_changed,
                                   const BITMAPINFO *shape_info, const void *shape_bits )
{
    struct ohos_window_surface *surface = get_ohos_surface(window_surface);
    struct native_buffer buffer;
    RECT rc;

    if (!perform(window_surface->hwnd, NATIVE_WINDOW_LOCK, &buffer, &rc))
    {
        const RECT *rgn_rect = surface->clip_rects, *end = surface->clip_rects + surface->clip_count;
        UINT alpha_mask = window_surface->alpha_mask, alpha_bits = window_surface->alpha_bits;
        COLORREF color_key = window_surface->color_key;
        BYTE alpha = alpha_bits >> 24;
        DWORD *src, *dst;
        int x, y, width;
        RECT locked;

        locked.left = rc.left;
        locked.top = rc.top;
        locked.right = rc.right;
        locked.bottom = rc.bottom;
        intersect_rect(&locked, &locked, rect);

        TRACE("hwnd %p, surface_rect %s, dirty %s, locked %s, surface %p\n", window_surface->hwnd,
              wine_dbgstr_rect(&rc), wine_dbgstr_rect(dirty), wine_dbgstr_rect(&locked), surface);

        /* Check pending_configure BEFORE dequeuing a buffer.
        * If a Wine-initiated resize is pending OS confirmation, skip the
        * entire flush.  SET_BUFFER_GEOMETRY has changed the buffer size
        * but SetWindowBounds (async) may not have updated the OHOS window
        * display area yet.  Dequeueing and then returning the buffer
        * (via cancelBuffer or FlushBuffer with empty region) is problematic:
        * - FlushBuffer with empty region still moves the buffer into the
        *   compositor's display queue, causing stretching
        * - cancelBuffer returning the buffer improperly can cause buffer
        *   leaks and yellow/uninitialized content
        * By checking before dequeue, we avoid both problems entirely. */
        {
            struct ohos_win_data *win_data = get_win_data(window_surface->hwnd);
            if (win_data)
            {
                if (win_data->pending_configure)
                {
                    /* If pending_configure has been set for too long without OS
                     * confirmation, clear it to avoid permanent render skip.
                     * This can happen when the OS does not send SURFACE_CHANGED
                     * for unchanged bounds, or the callback is lost. */
                    DWORD now = NtGetTickCount();
                    if (now - win_data->pending_configure_time > 500)
                    {
                        TRACE("hwnd %p: pending_configure timeout, clearing\n", window_surface->hwnd);
                        win_data->pending_configure = FALSE;
                        win_data->pending_configure_time = 0;
                    }
                    else
                    {
                        TRACE("hwnd %p: resize pending, skip flush to avoid stretching\n",
                              window_surface->hwnd);
                        release_win_data(win_data);
                        perform(window_surface->hwnd, NATIVE_WINDOW_UNLOCK_AND_POST, NULL);
                        return FALSE;
                    }
                }
                release_win_data(win_data);
            }
        }

        if (IsRectEmpty(&locked))
        {
            /* no dirty area, skip copy and just submit empty buffer */
            perform(window_surface->hwnd, NATIVE_WINDOW_UNLOCK_AND_POST, NULL);
            return TRUE;
        }

        BufferHandle *buffer_handle = buffer.buffer_handle;
        if (!buffer_handle) {
            ERR("failed to get buffer handle\n");
            return FALSE;
        }
        void *mapped_addr = mmap(buffer_handle->virAddr, buffer_handle->size,
            PROT_READ | PROT_WRITE, MAP_SHARED, buffer_handle->fd, 0);
        if (mapped_addr == MAP_FAILED) {
            ERR("failed to mmap buffer handle\n");
            return FALSE;
        }

        src = (DWORD *)color_bits + (locked.top - rect->top) * color_info->bmiHeader.biWidth + (locked.left - rect->left);
        dst = (DWORD *)mapped_addr + locked.top * buffer.stride + locked.left;
        width = min(locked.right - locked.left, buffer.width);

        for (y = locked.top; y < min(buffer.height, locked.bottom); y++) {
            if (alpha_mask)
                memcpy( dst, src, width * sizeof(*dst) );
            else if (alpha == 255)
                for (x = 0; x < width; x++) dst[x] = src[x] | 0xff000000;
            else
            {
#ifdef __ARM_NEON
                alpha_blend_neon_batch(dst, src, alpha, width);
#else
                for (x = 0; x < width; x++) {
                    DWORD blended = alpha_blend_lut_pixel(src[x], alpha);
                    if (color_key != CLR_INVALID && (src[x] & 0xffffff) == color_key)
                        blended = 0;
                    dst[x] = blended;
                }
#endif
            }

            if (color_key != CLR_INVALID && !alpha_mask && alpha != 255)
            {
                /* color_key already handled in LUT/NEON path above */
            }
            else if (color_key != CLR_INVALID)
                for (x = 0; x < width; x++)
                    if ((src[x] & 0xffffff) == color_key)
                        dst[x] = 0;

            if (rgn_rect) {
                while (rgn_rect < end && rgn_rect->bottom <= y)
                    rgn_rect++;
                apply_line_region(dst, width, locked.left, y, rgn_rect, end);
            }

            src += color_info->bmiHeader.biWidth;
            dst += buffer.stride;
        }

        /* Always pass the full locked rect as damage to the compositor.
         * OHOS NativeWindowFlushBuffer's region parameter is a constraint,
         * not a hint, the compositor only composites the specified region
         * and ignores the rest of the buffer.  Passing only the dirty rect
         * causes uninitialized (yellow/green) pixels outside the dirty area
         * and breaks edit controls that update small regions. */
        perform(window_surface->hwnd, NATIVE_WINDOW_UNLOCK_AND_POST, &locked);

        int result = munmap(mapped_addr, buffer_handle->size);
        if (result == -1) {
            ERR("failed to munmap buffer handle\n");
            return FALSE;
        }
    } else {
        TRACE("Unable to lock surface %p window %p buffer %d\n", surface, window_surface->hwnd, surface->widget_id);
    }

    return TRUE;
}

/***********************************************************************
 *           ohos_surface_destroy
 */
static void ohos_surface_destroy( struct window_surface *window_surface )
{
    struct ohos_window_surface *surface = get_ohos_surface( window_surface );

    TRACE( "freeing %p\n", surface );

    free( surface->clip_rects );
}

static const struct window_surface_funcs ohos_surface_funcs =
{
    ohos_surface_set_clip,
    ohos_surface_flush,
    ohos_surface_destroy
};


/***********************************************************************
 *           create_surface
 */
static struct window_surface *create_surface( HWND hwnd, const RECT *rect )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    struct ohos_window_surface *surface;
    int width = rect->right - rect->left;
    int height = rect->bottom - rect->top;
    struct window_surface *window_surface;

    TRACE("hwnd %p rect %s\n", hwnd, wine_dbgstr_rect(rect));

    memset( info, 0, sizeof(*info) );
    info->bmiHeader.biSize        = sizeof(info->bmiHeader);
    info->bmiHeader.biWidth       = width;
    info->bmiHeader.biHeight      = -height; /* top-down */
    info->bmiHeader.biPlanes      = 1;
    info->bmiHeader.biBitCount    = 32;
    info->bmiHeader.biSizeImage   = get_dib_image_size(info);
    info->bmiHeader.biCompression = BI_RGB;

    if ((window_surface = window_surface_create( sizeof(*surface), &ohos_surface_funcs, hwnd, rect, info, 0 )))
    {
        surface = get_ohos_surface( window_surface );
        surface->widget_id = get_window( hwnd );
    }

    return window_surface;
}

/***********************************************************************
 *           get_mono_icon_argb
 *
 * Return a monochrome icon/cursor bitmap bits in ARGB format.
 */
static unsigned int *get_mono_icon_argb( HDC hdc, HBITMAP bmp, unsigned int *width, unsigned int *height )
{
    BITMAP bm;
    char *mask;
    unsigned int i, j, stride, mask_size, bits_size, *bits = NULL, *ptr;

    if (!NtGdiExtGetObjectW( bmp, sizeof(bm), &bm )) return NULL;
    stride = ((bm.bmWidth + 15) >> 3) & ~1;
    mask_size = stride * bm.bmHeight;
    if (!(mask = malloc( mask_size ))) return NULL;
    if (!NtGdiGetBitmapBits( bmp, mask_size, mask )) goto done;

    bm.bmHeight /= 2;
    bits_size = bm.bmWidth * bm.bmHeight * sizeof(*bits);
    if (!(bits = malloc( bits_size ))) goto done;

    ptr = bits;
    for (i = 0; i < bm.bmHeight; i++)
        for (j = 0; j < bm.bmWidth; j++, ptr++)
        {
            int and = ((mask[i * stride + j / 8] << (j % 8)) & 0x80);
            int xor = ((mask[(i + bm.bmHeight) * stride + j / 8] << (j % 8)) & 0x80);
            if (!xor && and)
                *ptr = 0;
            else if (xor && !and)
                *ptr = 0xffffffff;
            else
                /* we can't draw "invert" pixels, so render them as black instead */
                *ptr = 0xff000000;
        }

    *width = bm.bmWidth;
    *height = bm.bmHeight;

done:
    free( mask );
    return bits;
}

/***********************************************************************
 *           get_bitmap_argb
 *
 * Return the bitmap bits in ARGB format. Helper for setting icons and cursors.
 */
static unsigned int *get_bitmap_argb( HDC hdc, HBITMAP color, HBITMAP mask, unsigned int *width,
                                      unsigned int *height )
{
    char buffer[FIELD_OFFSET( BITMAPINFO, bmiColors[256] )];
    BITMAPINFO *info = (BITMAPINFO *)buffer;
    BITMAP bm;
    unsigned int *ptr, *bits = NULL;
    unsigned char *mask_bits = NULL;
    int i, j;
    BOOL has_alpha = FALSE;

    if (!color) return get_mono_icon_argb( hdc, mask, width, height );

    if (!NtGdiExtGetObjectW( color, sizeof(bm), &bm )) return NULL;
    info->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info->bmiHeader.biWidth = bm.bmWidth;
    info->bmiHeader.biHeight = -bm.bmHeight;
    info->bmiHeader.biPlanes = 1;
    info->bmiHeader.biBitCount = 32;
    info->bmiHeader.biCompression = BI_RGB;
    info->bmiHeader.biSizeImage = bm.bmWidth * bm.bmHeight * 4;
    info->bmiHeader.biXPelsPerMeter = 0;
    info->bmiHeader.biYPelsPerMeter = 0;
    info->bmiHeader.biClrUsed = 0;
    info->bmiHeader.biClrImportant = 0;
    if (!(bits = malloc( bm.bmWidth * bm.bmHeight * sizeof(unsigned int) )))
        goto failed;
    if (!NtGdiGetDIBitsInternal( hdc, color, 0, bm.bmHeight, bits, info, DIB_RGB_COLORS, 0, 0 ))
        goto failed;

    *width = bm.bmWidth;
    *height = bm.bmHeight;

    for (i = 0; i < bm.bmWidth * bm.bmHeight; i++)
        if ((has_alpha = (bits[i] & 0xff000000) != 0)) break;

    if (!has_alpha)
    {
        unsigned int width_bytes = (bm.bmWidth + 31) / 32 * 4;
        /* generate alpha channel from the mask */
        info->bmiHeader.biBitCount = 1;
        info->bmiHeader.biSizeImage = width_bytes * bm.bmHeight;
        if (!(mask_bits = malloc( info->bmiHeader.biSizeImage ))) goto failed;
        if (!NtGdiGetDIBitsInternal( hdc, mask, 0, bm.bmHeight, mask_bits, info, DIB_RGB_COLORS, 0, 0 ))
            goto failed;
        ptr = bits;
        for (i = 0; i < bm.bmHeight; i++)
            for (j = 0; j < bm.bmWidth; j++, ptr++)
                if (!((mask_bits[i * width_bytes + j / 8] << (j % 8)) & 0x80)) *ptr |= 0xff000000;
        free( mask_bits );
    }

    return bits;

failed:
    free( bits );
    free( mask_bits );
    *width = *height = 0;
    return NULL;
}


/*
 * Wine-side cursor IDs: 1-based wrapper around ohos::adapter::OhosCursorType
 * (kDefault = 0, kResizeLeftRight = 22, ...). The +1 offset preserves
 * 0 as the "not a recognized system cursor; fall through to custom
 * ARGB" sentinel used by get_cursor_system_id(). set_cursor() subtracts
 * 1 before forwarding to SetCursorStyle().
 */
enum ohos_system_cursors
{
    TYPE_ARROW = 1,                                /* kDefault */
    TYPE_HORIZONTAL_DOUBLE_ARROW = 23,             /* kResizeLeftRight */
    TYPE_VERTICAL_DOUBLE_ARROW = 24,               /* kResizeUpDown */
    TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW = 12,     /* kNorthEastSouthWest */
    TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW = 13,      /* kNorthWestSouthEast */
    TYPE_CROSSHAIR = 14,                           /* kCross */
    TYPE_COPY = 15,                                /* kCursorCopy */
    TYPE_NO_DROP = 16,                             /* kCursorForbid */
    TYPE_HAND = 20,                                /* kHandPointing */
    TYPE_HELP = 21,                                /* kHelp */
    TYPE_ALL_SCROLL = 22,                          /* kMove */
    TYPE_TEXT = 27,                                /* kTextCursor */
    TYPE_ZOOM_IN = 28,                             /* kZoomIn */
    TYPE_ZOOM_OUT = 29,                            /* kZoomOut */
    TYPE_GRAB = 19,                                /* kHandOpen */
    TYPE_GRABBING = 18,                            /* kHandGrabbing */
    TYPE_ALIAS = 15,                               /* no dedicated alias style, reuse Copy */
    TYPE_WAIT = 43,                                /* kLoading */
    TYPE_APPSTARTING = 44,                          /* kRunning */
    TYPE_CONTEXT_MENU = 1,                         /* no equivalent, fall back to Arrow */
    TYPE_CELL = 1,                                 /* no equivalent, fall back to Arrow */
    TYPE_VERTICAL_TEXT = 40,                       /* kHorizontalTextCursor */
};

struct system_cursors
{
    WORD id;
    enum ohos_system_cursors ohos_id;
};

static const struct system_cursors user32_cursors[] =
{
    { OCR_NORMAL,      TYPE_ARROW },
    { OCR_IBEAM,       TYPE_TEXT },
    { OCR_WAIT,        TYPE_WAIT },
    { OCR_CROSS,       TYPE_CROSSHAIR },
    { OCR_SIZE,        TYPE_ALL_SCROLL },
    { OCR_SIZEALL,     TYPE_ALL_SCROLL },
    { OCR_SIZENWSE,    TYPE_TOP_LEFT_DIAGONAL_DOUBLE_ARROW },
    { OCR_SIZENESW,    TYPE_TOP_RIGHT_DIAGONAL_DOUBLE_ARROW },
    { OCR_SIZEWE,      TYPE_HORIZONTAL_DOUBLE_ARROW },
    { OCR_SIZENS,      TYPE_VERTICAL_DOUBLE_ARROW },
    { OCR_NO,          TYPE_NO_DROP },
    { OCR_HAND,        TYPE_HAND },
    /*
     * OCR_APPSTARTING represents background activity (arrow with progress),
     * so map it to kRunning instead of treating it as a custom ARGB cursor.
     * This keeps sizing under the system pointer style and avoids applying
     * display-density scaling to the Windows bitmap.
     */
    { OCR_APPSTARTING, TYPE_APPSTARTING },
    { OCR_HELP,        TYPE_HELP },
    { 0 }
};

static const struct system_cursors comctl32_cursors[] =
{
    /* 102 TYPE_MOVE doesn't exist */
    { 104, TYPE_COPY },
    { 105, TYPE_ARROW },
    { 106, TYPE_HORIZONTAL_DOUBLE_ARROW },
    { 107, TYPE_HORIZONTAL_DOUBLE_ARROW },
    { 108, TYPE_GRABBING },
    { 135, TYPE_VERTICAL_DOUBLE_ARROW },
    { 0 }
};

static const struct system_cursors ole32_cursors[] =
{
    { 1, TYPE_NO_DROP },
    /* 2 TYPE_MOVE doesn't exist */
    { 3, TYPE_COPY },
    { 4, TYPE_ALIAS },
    { 0 }
};

static const struct system_cursors riched20_cursors[] =
{
    { 105, TYPE_GRABBING },
    { 109, TYPE_COPY },
    /* 110 TYPE_MOVE doesn't exist */
    { 111, TYPE_NO_DROP },
    { 0 }
};

static const struct
{
    const struct system_cursors *cursors;
    WCHAR name[16];
} module_cursors[] =
{
    { user32_cursors, {'u','s','e','r','3','2','.','d','l','l',0} },
    { comctl32_cursors, {'c','o','m','c','t','l','3','2','.','d','l','l',0} },
    { ole32_cursors, {'o','l','e','3','2','.','d','l','l',0} },
    { riched20_cursors, {'r','i','c','h','e','d','2','0','.','d','l','l',0} }
};

static int get_cursor_system_id( const ICONINFOEXW *info )
{
    const struct system_cursors *cursors;
    const WCHAR *module;
    unsigned int i;

    if (info->szResName[0]) return 0;  /* only integer resources are supported here */

    if ((module = wcsrchr( info->szModName, '\\' ))) module++;
    else module = info->szModName;
    for (i = 0; i < ARRAY_SIZE( module_cursors ); i++)
        if (!wcsicmp( module, module_cursors[i].name )) break;
    if (i == ARRAY_SIZE( module_cursors )) return 0;

    cursors = module_cursors[i].cursors;
    for (i = 0; cursors[i].id; i++)
        if (cursors[i].id == info->wResID) return cursors[i].ohos_id;

    return 0;
}

RECT get_desktop_rect(void)
{
    RECT desktop_rect;
    struct window_bounds bounds = GetDisplayAvailArea();
    desktop_rect.left = bounds.left;
    desktop_rect.top = bounds.top;
    desktop_rect.right = bounds.left + bounds.width;
    desktop_rect.bottom = bounds.top + bounds.height;

    TRACE("%s\n", wine_dbgstr_rect(&desktop_rect));
    return desktop_rect;
}

void OHOS_SetDesktopWindow(HWND hwnd)
{
    unsigned int width, height;

    TRACE("%p\n", hwnd);

    /* retrieve the real size of the desktop */
    SERVER_START_REQ(get_window_rectangles)
    {
        req->handle = wine_server_user_handle(hwnd);
        req->relative = COORDS_CLIENT;
        wine_server_call(req);
        width  = reply->window.right;
        height = reply->window.bottom;
    }
    SERVER_END_REQ;

    TRACE("%d, %d\n", width, height);

    if ( !width || !height)  /* not initialized yet */
    {
        RECT desktop_rect = get_desktop_rect();
        width = desktop_rect.right - desktop_rect.left;
        height = desktop_rect.bottom - desktop_rect.top;
        SERVER_START_REQ(set_window_pos)
        {
            req->handle        = wine_server_user_handle(hwnd);
            req->previous      = 0;
            req->swp_flags     = SWP_NOZORDER;
            req->window.left   = desktop_rect.left;
            req->window.top    = desktop_rect.top;
            req->window.right  = desktop_rect.right;
            req->window.bottom = desktop_rect.bottom;
            req->client        = req->window;
            wine_server_call(req);
        }
        SERVER_END_REQ;
    }
}

LRESULT OHOS_DesktopWindowProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    TRACE( "%p msg:%x, wp: %lx, lp: %lx\n", hwnd, msg, wp, lp );

    switch (msg)
    {
    case WM_PARENTNOTIFY:
        if (LOWORD(wp) == WM_DESTROY) destroy_native_window( (HWND)lp );
        break;
    }
    return NtUserMessageCall( hwnd, msg, wp, lp, 0, NtUserDefWindowProc, FALSE );
}


/***********************************************************************
 *           OHOS_ProcessEvents
 */
BOOL OHOS_ProcessEvents( DWORD mask )
{
    if (GetCurrentThreadId() == desktop_tid)
    {
        /* don't process nested events */
        if (current_event) mask = 0;
        return process_events( mask );
    }
    return FALSE;
}

/**********************************************************************
 *           OHOS_CreateWindow
 */
BOOL OHOS_CreateWindow( HWND hwnd )
{
    TRACE( "%p\n", hwnd );

    if (hwnd == NtUserGetDesktopWindow())
    {
        struct ohos_win_data *data;

        init_event_queue();
        if (!(data = alloc_win_data( hwnd ))) return FALSE;
        release_win_data( data );
    }

    if ((event_pipe[0] == 0 || event_pipe[1] == 0)) {
        init_event_queue();
    }
    return TRUE;
}


/***********************************************************************
 *           OHOS_DestroyWindow
 */
void OHOS_DestroyWindow( HWND hwnd )
{
    struct ohos_win_data *data;

    TRACE("%p\n", hwnd);

    if (!(data = get_win_data( hwnd ))) return;
    free_win_data( data );
}

/***********************************************************************
 *           create_win_data
 *
 * Create a data window structure for an existing window.
 */
static struct ohos_win_data *create_win_data( HWND hwnd, const struct window_rects *rects )
{
    struct ohos_win_data *data;
    HWND parent;

    /* Don't create win data for desktop or HWND_MESSAGE windows. */
    if (!(parent = NtUserGetAncestor(hwnd, GA_PARENT))) return NULL;
    if (parent != NtUserGetDesktopWindow() && !NtUserGetAncestor(parent, GA_PARENT))
        return NULL;
    if (NtUserGetWindowThread(hwnd, NULL) != GetCurrentThreadId()) return NULL;

    if (!(data = alloc_win_data( hwnd ))) return NULL;

    data->parent = (parent == NtUserGetDesktopWindow()) ? 0 : parent;
    data->rects = *rects;
    data->ignored = in_child_process;

    if (parent == NtUserGetDesktopWindow())
    {
        if (data->ignored)
        {
            data->widget_id = -1;
            FIXME("win %p/%p window %s whole %s client %s in child process\n",
                hwnd, parent, wine_dbgstr_rect(&data->rects.window),
                wine_dbgstr_rect(&data->rects.visible), wine_dbgstr_rect(&data->rects.client));
        }
        else
        {
            create_native_window( data );
            TRACE("win %p/%p %d window %s whole %s client %s\n",
                hwnd, parent, data->widget_id, wine_dbgstr_rect( &data->rects.window ),
                wine_dbgstr_rect( &data->rects.visible ), wine_dbgstr_rect( &data->rects.client));
        }
    }
    else
    {
        data->widget_id = -1;
        FIXME("win %p/%p window %s whole %s client %s\n",
               hwnd, parent, wine_dbgstr_rect(&data->rects.window),
               wine_dbgstr_rect(&data->rects.visible), wine_dbgstr_rect(&data->rects.client));
    }

    return data;
}

static void show_window(struct ohos_win_data *data)
{
    if (data->widget_id > 0)
    {
        HWND prev = NULL;
        HWND next = NULL;
        int prev_window = -1;
        int next_window = -1;

        /* find window that this one must be after */
        prev = NtUserGetWindowRelative(data->hwnd, GW_HWNDPREV);
        while (prev && !((NtUserGetWindowLongW(prev, GWL_STYLE) & (WS_VISIBLE | WS_MINIMIZE)) == WS_VISIBLE &&
                         (prev_window = get_window(prev))))
            prev = NtUserGetWindowRelative(prev, GW_HWNDPREV);
        if (!prev_window)
        {
            /* find window that this one must be before */
            next = NtUserGetWindowRelative(data->hwnd, GW_HWNDNEXT);
            while (next && !((NtUserGetWindowLongW(next, GWL_STYLE) & (WS_VISIBLE | WS_MINIMIZE)) == WS_VISIBLE &&
                             (next_window = get_window(next))))
                next = NtUserGetWindowRelative(next, GW_HWNDNEXT);
        }

        TRACE("win %p/%d below hwnd %p/%d, above hwnd %p/%d\n",
              data->hwnd, data->widget_id, prev, prev_window, next, next_window);

        /* Flush a white frame before showing the window to prevent
         * the compositor from stretching a stale/wrong-sized buffer.
         * When the same hwnd is destroy -> created with different sizes
         * (dropdown list switching), SetWindowBounds resizes the OHOS
         * window before SetWindowVisible makes it visible.  Without a
         * correctly-sized buffer in the compositor queue, it stretches
         * the old (smaller) buffer to fill the new (larger) window.
         * This pre-show white flush ensures the compositor has a
         * correctly-sized buffer, eliminating the stretch.
         * Only flush when pending_configure is set (size changed),
         * otherwise the old buffer size matches the window and no
         * stretching can occur ¡ª a white flush would only cause an
         * unnecessary flash. */
        if (!data->on_screen && data->pending_configure)
        {
            struct native_buffer buffer;
            RECT rc;
            /* Clear pending_configure so that the subsequent Wine flush
             * (with actual content) is not skipped.  The pre-show white
             * flush ensures the compositor has a correctly-sized buffer,
             * so there is no risk of stretching anymore. */
            data->pending_configure = FALSE;
            data->pending_configure_time = 0;
            if (!perform(data->hwnd, NATIVE_WINDOW_LOCK, &buffer, &rc))
            {
                BufferHandle *buffer_handle = buffer.buffer_handle;
                if (buffer_handle)
                {
                    void *mapped_addr = mmap(buffer_handle->virAddr, buffer_handle->size,
                        PROT_READ | PROT_WRITE, MAP_SHARED, buffer_handle->fd, 0);
                    if (mapped_addr != MAP_FAILED)
                    {
                        DWORD *bits = (DWORD *)mapped_addr;
                        int y;
                        for (y = 0; y < buffer.height; y++)
                        {
                            int x;
                            for (x = 0; x < buffer.width; x++)
                                bits[x] = 0xFFFFFFFF;
                            bits += buffer.stride;
                        }
                        munmap(mapped_addr, buffer_handle->size);
                    }
                }
                perform(data->hwnd, NATIVE_WINDOW_UNLOCK_AND_POST, &rc);
            }
        }

        set_window_visible(data->hwnd, TRUE);
        data->on_screen = TRUE;
        data->has_shown = TRUE;
    }
}

static void hide_window(struct ohos_win_data *data)
{
    TRACE("win %p/%d\n", data->hwnd, data->widget_id);

    if (data->widget_id > 0)
        set_window_visible(data->hwnd, FALSE);
    data->on_screen = FALSE;
}

static struct ohos_client_surface *impl_from_client_surface( struct client_surface *client )
{
    return CONTAINING_RECORD( client, struct ohos_client_surface, client );
}

static void ohos_client_surface_destroy( struct client_surface *client )
{
    struct ohos_client_surface *surface = impl_from_client_surface(client);

    TRACE( "%s\n", debugstr_client_surface( client ) );

    if (surface->widget_id > 0) 
        destroy_native_window( surface->hwnd );
}

static void ohos_client_surface_detach( struct client_surface *client )
{
    TRACE( "%s\n", debugstr_client_surface( client ) );
}

static void ohos_client_surface_update( struct client_surface *client )
{
    TRACE( "%s\n", debugstr_client_surface( client ) );
}

static void ohos_client_surface_present( struct client_surface *client, HDC hdc )
{
    TRACE( "%s\n", debugstr_client_surface( client ) );
}

static const struct client_surface_funcs ohos_client_surface_funcs =
{
    .destroy = ohos_client_surface_destroy,
    .detach = ohos_client_surface_detach,
    .update = ohos_client_surface_update,
    .present = ohos_client_surface_present,
};

struct ohos_client_surface *ohos_client_surface_create( HWND hwnd, BOOL force )
{
    struct ohos_client_surface *surface;

    if (!(surface = client_surface_create( sizeof(*surface), &ohos_client_surface_funcs, hwnd ))) return NULL;

    NtUserGetClientRect( hwnd, &surface->rect, NtUserGetDpiForWindow( hwnd ) );
    if (!force) return surface;

    // TODO: Implement the nesting of window surface into client surface.
    surface->widget_id = create_client_window( hwnd );
    if (surface->widget_id <= 0)
    {
        ERR("Failed to create client surface\n");
        client_surface_release(&surface->client);
        return NULL;
    }

    return surface;
}

/***********************************************************************
 *           OHOS_WindowPosChanging
 */
BOOL OHOS_WindowPosChanging( HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects )
{
    struct ohos_win_data *data = get_win_data( hwnd );
    BOOL ret = FALSE;

    TRACE( "hwnd %p, swp_flags %#x, shaped %u, rects %s\n", hwnd, swp_flags, shaped, debugstr_window_rects( rects ) );

    if (!data && !(data = create_win_data( hwnd, rects ))) return FALSE; /* use default surface */

    // Defer window creation for the top-level window until needed
    if (!data->parent)
    {
        if (!IsRectEmpty(&rects->window) && data->lazy_created && data->widget_id <= 0) {
            data->rects = *rects;
            data->lazy_created = FALSE;
            create_native_window( data );
            TRACE("lazy create win %p %d window_rects %s\n", hwnd, data->widget_id, debugstr_window_rects( &data->rects ));
        }
    }

    ret = data->widget_id > 0;
    release_win_data(data);

    TRACE( "hwnd %p, ret %d\n", hwnd, ret );
    return ret;
}


/***********************************************************************
 *           OHOS_CreateWindowSurface
 */
BOOL OHOS_CreateWindowSurface( HWND hwnd, BOOL layered, const RECT *surface_rect, struct window_surface **surface )
{
    struct window_surface *previous;
    struct ohos_win_data *data;

    TRACE( "hwnd %p, layered %u, surface_rect %s, surface %p\n", hwnd, layered, wine_dbgstr_rect( surface_rect ), surface );

    if ((previous = *surface) && previous->funcs == &ohos_surface_funcs) return TRUE;
    if (!(data = get_win_data( hwnd ))) return TRUE; /* use default surface */
    if (previous) window_surface_release( previous );

    *surface = create_surface( data->hwnd, surface_rect );

    TRACE( "hwnd %p, surface %p ok\n", hwnd, *surface );

    release_win_data( data );
    return TRUE;
}


/***********************************************************************
 *           OHOS_WindowPosChanged
 */
void OHOS_WindowPosChanged( HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags, BOOL fullscreen,
                               const struct window_rects *new_rects, struct window_surface *surface )
{
    struct ohos_win_data *data;
    UINT new_style = NtUserGetWindowLongW( hwnd, GWL_STYLE );
    struct window_rects old_rects;

    if (!(data = get_win_data( hwnd ))) return;

    old_rects = data->rects;
    data->rects = *new_rects;

    TRACE("win %p/%d new_rects %s style %08x flags %08x surface %p\n", hwnd, data->widget_id,
          debugstr_window_rects(new_rects), new_style, swp_flags, surface);

    if (data->widget_id <= 0) goto done;

    if (data->on_screen)
    {
        if ((swp_flags & SWP_HIDEWINDOW) && !(new_style & WS_VISIBLE))
        {
            hide_window(data);
            goto done;
        }
    }

    if (data->pending_change)
    {
        data->pending_change = FALSE;
        WARN("Maximize, minimize, and restore are handled by the OS, with duplicate events filtered.\n");
    }
    else if (swp_flags & SWP_NOSENDCHANGING)
    {
        TRACE("hwnd %p: OS-initiated change via GetWindowStateUpdates, skip window_pos_changed\n", hwnd);
    }
    else
    {
        int old_width = old_rects.visible.right - old_rects.visible.left;
        int old_height = old_rects.visible.bottom - old_rects.visible.top;
        int new_width = new_rects->visible.right - new_rects->visible.left;
        int new_height = new_rects->visible.bottom - new_rects->visible.top;
        if (old_width != new_width || old_height != new_height)
        {
            TRACE("window size changed %d-%d --> %d-%d\n", old_width, old_height, new_width, new_height);
            if (data->has_shown)
            {
                // surface can only be requested for shown window.
                perform(hwnd, SET_BUFFER_GEOMETRY, new_width, new_height);
            }
            data->buffer_width = new_width;
            data->buffer_height = new_height;

            data->pending_configure = TRUE;
            data->pending_configure_time = NtGetTickCount();
            data->pending_rect = new_rects->window;
        }
        window_pos_changed( hwnd, new_rects, &old_rects, new_style, swp_flags );
    }

    if (new_style & WS_VISIBLE)
    {
        if (data->widget_id > 0)
        {
            if (!data->on_screen || (swp_flags & (SWP_FRAMECHANGED | SWP_STATECHANGED)))
                set_window_properties(data);

            if (!data->on_screen &&
                (!(NtUserGetWindowLongW( hwnd, GWL_EXSTYLE ) & WS_EX_LAYERED)))
                show_window(data);
        }
        else if (!data->on_screen)
            show_window(data);
    }

    if (!data->on_screen)
    {
        if (swp_flags & SWP_SHOWWINDOW)
            show_window(data);
    }

done:
    release_win_data(data);
}

/***********************************************************************
 *              SetWindowStyle
 *
 */
void OHOS_SetWindowStyle(HWND hwnd, INT offset, STYLESTRUCT *style)
{
    struct ohos_win_data *data;

    TRACE("hwnd %p offset %d styleOld 0x%08x styleNew 0x%08x\n", hwnd, offset, style->styleOld, style->styleNew);

    if (hwnd == NtUserGetDesktopWindow()) return;
    if (!(data = get_win_data(hwnd))) return;

    if (data->widget_id > 0)
    {
        DWORD changed = style->styleNew ^ style->styleOld;

        set_window_properties(data);

        if (offset == GWL_EXSTYLE && (changed & WS_EX_LAYERED)) /* changing WS_EX_LAYERED resets attributes */
        {
            data->layered = FALSE;
            sync_window_opacity(data, 0, FALSE, 0);
        }
    }

    release_win_data(data);
}

/*****************************************************************
 *              SetWindowText
 */
void OHOS_SetWindowText(HWND hwnd, LPCWSTR text)
{
    struct ohos_win_data *data = get_win_data(hwnd);
    DWORD text_len;
    DWORD utf8_count;
    char *utf8 = NULL;

    TRACE("%p, %s\n", hwnd, debugstr_w(text));

    if (!data || data->widget_id <= 0) goto done;

    text_len = (lstrlenW(text) + 1) * sizeof(WCHAR);

    if (!RtlUnicodeToUTF8N(NULL, 0, &utf8_count, text, text_len) &&
        (utf8 = malloc(utf8_count)))
    {
        RtlUnicodeToUTF8N(utf8, utf8_count, &utf8_count, text, text_len);
        SetWindowTitle(get_widget_id(hwnd), utf8);
    }

done:
    if (utf8) free(utf8);
    release_win_data(data);
}

/***********************************************************************
 *           OHOS_ShowWindow
 */
UINT OHOS_ShowWindow( HWND hwnd, INT cmd, RECT *rect, UINT swp )
{
    struct ohos_win_data *data = get_win_data(hwnd);
    int widget_id;

    TRACE("win %p/%d cmd %d at %s flags %08x\n",
          hwnd, data ? data->widget_id : -1, cmd, wine_dbgstr_rect(rect), swp);

    if (!data || data->widget_id <= 0) goto done;
    widget_id = get_widget_id(data->hwnd);

    switch (cmd)
    {
    case SW_MINIMIZE:
    case SW_SHOWMINIMIZED:
        MinimizeWindow(widget_id);
        /* hide icons off-screen (standard Windows minimize behavior) */
        if (rect->left != -32000 || rect->top != -32000)
        {
            OffsetRect( rect, -32000 - rect->left, -32000 - rect->top );
            swp &= ~(SWP_NOMOVE | SWP_NOCLIENTMOVE);
        }
        data->pending_change = TRUE;
        break;
    case SW_MAXIMIZE: /* = SW_SHOWMAXIMIZED (value 3) */
        MaximizeWindow(widget_id);
        data->pending_change = TRUE;
        break;
    case SW_RESTORE:
        RestoreWindow(widget_id);
        data->pending_change = TRUE;
        break;
    default:
        break;
    }

    TRACE("win %p rect %s flags %08x\n", hwnd, wine_dbgstr_rect(rect), swp);
done:
    release_win_data(data);
    return swp;
}


/*****************************************************************
 *	     OHOS_SetParent
 */
void OHOS_SetParent( HWND hwnd, HWND parent, HWND old_parent )
{
    DWORD style, ex_style;
    struct ohos_win_data *data;

    if (parent == old_parent) return;
    if (!(data = get_win_data( hwnd ))) return;

    if (data->ignored)
    {
        FIXME("hwnd %p parent %p -> %p in child process\n", data->hwnd, old_parent, parent);
        goto done;
    }

    style = NtUserGetWindowLongW(data->hwnd, GWL_STYLE);
    ex_style = NtUserGetWindowLongW(data->hwnd, GWL_EXSTYLE);

    TRACE( "hwnd %p parent %p -> %p style %08x ex_style %08x window_rects %s\n",
        data->hwnd, old_parent, parent, style, ex_style, debugstr_window_rects(&data->rects) );

    data->parent = (parent == NtUserGetDesktopWindow()) ? 0 : parent;
    if (parent != NtUserGetDesktopWindow()) /* a child window */
    {
        if (old_parent == NtUserGetDesktopWindow())
        {
            /* destroy the current window's old OHOS peer */
            destroy_native_window(data->hwnd);
            /* Paired cleanup: destroy_native_window() only closes the native
             * widget and frees native_win_data; it does not touch ohos_win_data.
             * Leaving data->window set leaves a dangling handle, so any later
             * show_window() call uses a dead handle in set_window_visible()
             * (no-op) and the window never appears after a top-to-child
             * reparent. Mirror free_win_data() and reset both fields here. */
            data->widget_id = -1;
            data->on_screen = FALSE;
        }
    }
    else  /* new top level window */
    {
        create_native_window(data);
    }

done:
    release_win_data(data);
}


/***********************************************************************
 *           OHOS_SetCapture
 */
void OHOS_SetCapture( HWND hwnd, UINT flags )
{
    if (!(flags & (GUI_INMOVESIZE | GUI_INMENUMODE))) return;
    set_capture( hwnd );
}


static BOOL get_icon_info( HICON handle, ICONINFOEXW *ret )
{
    UNICODE_STRING module, res_name;
    ICONINFO info;

    module.Buffer = ret->szModName;
    module.MaximumLength = sizeof(ret->szModName) - sizeof(WCHAR);
    res_name.Buffer = ret->szResName;
    res_name.MaximumLength = sizeof(ret->szResName) - sizeof(WCHAR);
    if (!NtUserGetIconInfo( handle, &info, &module, &res_name, NULL, 0 )) return FALSE;
    ret->fIcon    = info.fIcon;
    ret->xHotspot = info.xHotspot;
    ret->yHotspot = info.yHotspot;
    ret->hbmColor = info.hbmColor;
    ret->hbmMask  = info.hbmMask;
    ret->wResID   = res_name.Length ? 0 : LOWORD(res_name.Buffer);
    ret->szModName[module.Length] = 0;
    ret->szResName[res_name.Length] = 0;
    return TRUE;
}


/***********************************************************************
 *           OHOS_SetCursor
 */
void OHOS_SetCursor( HWND hwnd, HCURSOR handle )
{
    if (handle)
    {
        unsigned int width = 0, height = 0, *bits = NULL;
        ICONINFOEXW info;
        int id;

        if (!get_icon_info( handle, &info )) return;

        if (!(id = get_cursor_system_id( &info )))
        {
            HDC hdc = NtGdiCreateCompatibleDC( 0 );
            bits = get_bitmap_argb( hdc, info.hbmColor, info.hbmMask, &width, &height );
            NtGdiDeleteObjectApp( hdc );

            /* make sure hotspot is valid */
            if (info.xHotspot >= width || info.yHotspot >= height)
            {
                info.xHotspot = width / 2;
                info.yHotspot = height / 2;
            }
        }
        set_cursor( hwnd, id, width, height, info.xHotspot, info.yHotspot, bits );
        free( bits );
        NtGdiDeleteObjectApp( info.hbmColor );
        NtGdiDeleteObjectApp( info.hbmMask );
    }
    /*
     * handle == NULL means the application asked to clear the cursor for
     * this window. Forward hwnd (not 0) so set_cursor() can locate the
     * native widget and revert it to the default arrow; passing 0 caused
     * get_native_win_data() to fail and left whatever custom cursor the
     * application last set (e.g. column-resize, column-select) stuck on
     * screen even after the pointer moved away.
     */
    else set_cursor( hwnd, 0, 0, 0, 0, 0, NULL );
}

/**********************************************************************
 *           OHOS_WindowMessage
 */
LRESULT OHOS_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp )
{
    switch (msg)
    {
    case WM_OHOS_REFRESH:
        NtUserExposeWindowSurface( hwnd, 0, NULL, 0 );
        return 0;
    default:
        FIXME( "got window msg %x hwnd %p wp %lx lp %lx\n", msg, hwnd, (long)wp, lp );
        return 0;
    }
}
