#ifndef __WINE_OHOS_H
#define __WINE_OHOS_H

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <pthread.h>

#include "ohos_native.h"

#include "windef.h"
#include "winbase.h"
#include "ntgdi.h"
#include "wine/gdi_driver.h"
#include "unixlib.h"
#include "wine/debug.h"

/* ohos private window data */
struct ohos_win_data
{
    HWND                hwnd;          /* hwnd that this private data belongs to */
    HWND                parent;        /* parent hwnd for child windows */
    struct window_rects rects;         /* window rects in monitor DPI, relative to parent client area */
    int                 widget_id;     /* native window id */
    int                 client_widget_id; /* nested client surface (vulkan), 0 if none */
    unsigned int        on_screen;     /* is window ordered in? (minimized or not) */
    unsigned int        shaped;        /* is window using a custom region shape? */
    unsigned int        layered;       /* is window layered and with valid attributes? */
    unsigned int        minimized;     /* is window minimized? */
    unsigned int        lazy_created;
    unsigned int        ignored;
    unsigned int        pending_change;
    unsigned int        pending_configure;  /* Wine-initiated size change, awaiting OS confirmation */
    DWORD               pending_configure_time; /* tick when pending_configure was set */
    unsigned int        vulkan_active;  /* is a Vulkan client surface attached? (informational; GDI flush stays unclipped) */
    RECT                pending_rect;       /* Wine-initiated target rect */
    int                 buffer_width;       /* current buffer width */
    int                 buffer_height;      /* current buffer height */
    RECT                os_pending_rect;    /* OS callback pending rect */
    int                 os_pending_reason;  /* OS callback RectChange reason */
    BOOL                os_rect_pending;    /* whether OS rect change is pending */
    unsigned int        has_shown;          /* is window has already called show(), Only shown windows can request surface */
    DWORD               last_resize_time;   /* tick of last window size change, used to hold frames during live resize */
    unsigned int        refresh_pending;    /* WM_OHOS_REFRESH reflush armed during a resize hold */
    unsigned int        remote_gdi_recreate_pending; /* shared-surface rebuild deferred across a resize burst */
    RECT                remote_gdi_pending_rect;     /* surface rect to rebuild the shared surface with on settle */
    BOOL                remote_gdi_visible;          /* current overlay visibility (tab switching etc.) */
};

/* identity of a native (OHOS) window stored as a window property on the
 * toplevel: pure values only, readable from every wine process through the
 * wineserver (the X11 whole_window_prop pattern).  The property packs
 * MAKELONGLONG( widget_id, owner_pid ). */
extern const WCHAR ohos_window_id_prop[];
struct ohos_window_identity { int owner_pid; int widget_id; };

/* Find this process's host toplevel window by widget id (overlay notification side). */
extern HWND ohos_find_toplevel_by_widget_id( int widget_id );
/* Wake the page owner for compositing: the owner only composites inside its
 * own GDI flush, and when a child's shared surface gets new content
 * (BLIT/UPDATE) the host usually does not repaint on its own, so post it a
 * WM_OHOS_REFRESH to trigger an expose and a refresh.  Call only from a wine
 * thread (desktop/window thread) -- NtUserPostMessage needs a valid TEB; the
 * IPC thread must hand over via send_event(OVERLAY_DIRTY_EVENT) to the
 * desktop thread.  Deduplicates through refresh_pending. */
extern void ohos_wake_page_composite( int parent_widget_id );

struct ohos_client_surface
{
    struct client_surface client;
    HWND                  hwnd;
    int                   widget_id;
    RECT                  rect;
    ohos_native_window    native_window; /* imported producer (cross-process), NULL when local */
    BOOL                  is_remote;     /* widget_id belongs to the host process */
};

extern struct ohos_win_data *alloc_win_data( HWND hwnd );
extern void free_win_data( struct ohos_win_data *data );
extern struct ohos_win_data *get_win_data( HWND hwnd );
extern void release_win_data( struct ohos_win_data *data );
extern struct ohos_client_surface *ohos_client_surface_create( HWND hwnd, BOOL force );

/* cross-process embedding (SetParent across wine processes) */
extern struct ohos_window_identity ohos_get_foreign_toplevel( HWND hwnd );
extern BOOL ohos_create_remote_gdi_surface( struct ohos_win_data *data, const RECT *surface_rect );
extern void ohos_destroy_remote_gdi_surface( HWND hwnd );

/* remote GDI shared-surface management (device.c): pixels live in an
 * ashmem buffer shared with the page owner, which composites them */

/* op codes of the overlay notify callback (mirrors the producer side in
 * wine-adapter new_ipc) */
enum remote_gdi_op
{
    REMOTE_GDI_OP_ADD     = 0,  /* overlay registered */
    REMOTE_GDI_OP_BLIT    = 1,  /* new frame in the shared buffer */
    REMOTE_GDI_OP_REMOVE  = 2,  /* overlay unregistered */
    REMOTE_GDI_OP_UPDATE  = 3,  /* position-only change */
    REMOTE_GDI_OP_VISIBLE = 4,  /* visibility change (off_x carries 0/1) */
};

extern BOOL has_remote_gdi_producer( HWND hwnd );
extern int host_gdi_widget_id( HWND hwnd );
extern int attach_remote_gdi_producer( HWND hwnd, void *pixels, int width,
                                       int height, int stride, int shared_id,
                                       int scale_num, int scale_den );
extern int get_remote_gdi_info( HWND hwnd, int *shared_id,
                                int *scale_num, int *scale_den );
/* Fetch the logical->raw scale; the shared surface need not be attached (the
 * last attach's value survives detach: a deferred rebuild of a hidden window
 * has no raw surface rect to work from). */
extern int get_remote_gdi_scale( HWND hwnd, int *scale_num, int *scale_den );
extern BOOL detach_remote_gdi_producer( HWND hwnd, int *shared_id );
extern void wineohos_register_gdi_overlay(void);

extern void create_native_window( struct ohos_win_data *data );
extern void destroy_native_window( HWND hwnd );
extern int window_pos_changed( HWND hwnd, const struct window_rects *rects,
                               const struct window_rects *old_rects,
                               UINT style, UINT flags );
extern int set_capture( HWND hwnd );
extern int set_cursor( HWND hwnd, int id, int width, int height,
                       int hotspotx, int hotspoty, const unsigned int *bits );
extern int set_window_features( HWND hwnd, const struct window_features* wf);
extern int set_window_state( HWND hwnd, const struct window_state* state);
extern int set_window_visible( HWND hwnd, int visible);
extern void set_window_title( HWND hwnd, const WCHAR *text);
extern BOOL is_tooltips_hwnd(HWND hwnd);
extern int perform( HWND hwnd, int operation, ... );
extern int cancelBuffer( HWND hwnd, int buffer_id );

extern pthread_mutex_t win_data_mutex;
extern void OHOS_SetCursor( HWND hwnd, HCURSOR handle );
extern BOOL OHOS_CreateDesktop( const WCHAR *name, UINT width, UINT height );
extern void OHOS_SetDesktopWindow(HWND hwnd);
extern BOOL OHOS_CreateWindow( HWND hwnd );
extern void OHOS_DestroyWindow( HWND hwnd );
extern BOOL OHOS_ProcessEvents( DWORD mask );
extern LRESULT OHOS_DesktopWindowProc( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
extern void OHOS_SetParent( HWND hwnd, HWND parent, HWND old_parent );
extern void OHOS_SetCapture( HWND hwnd, UINT flags );
extern UINT OHOS_ShowWindow( HWND hwnd, INT cmd, RECT *rect, UINT swp );
extern LRESULT OHOS_WindowMessage( HWND hwnd, UINT msg, WPARAM wp, LPARAM lp );
extern BOOL init_ohos_pasteboard(void);
extern void OHOS_UpdateClipboard(void);
extern LRESULT OHOS_ClipboardWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
extern BOOL OHOS_WindowPosChanging( HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects );
extern BOOL OHOS_GetWindowStateUpdates( HWND hwnd, UINT *state_cmd, UINT *config_cmd, RECT *rect, HWND *foreground );
extern BOOL OHOS_CreateWindowSurface( HWND hwnd, BOOL layered, const RECT *surface_rect, struct window_surface **surface );
extern void OHOS_WindowPosChanged( HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags, BOOL fullscreen,
                                      const struct window_rects *new_rects, struct window_surface *surface );

extern UINT OHOS_OpenGLInit( UINT version, const struct opengl_funcs *opengl_funcs, const struct opengl_driver_funcs **driver_funcs );
extern UINT OHOS_VulkanInit( UINT version, void *vulkan_handle, const struct vulkan_driver_funcs **driver_funcs );

extern unsigned int screen_width;
extern unsigned int screen_height;
extern RECT virtual_screen_rect;
extern MONITORINFOEXW default_monitor;
extern BOOL in_child_process;

enum ohos_window_messages
{
    WM_OHOS_REFRESH = WM_WINE_FIRST_DRIVER_MSG,
};

extern struct window_state get_window_state(HWND hwnd);
extern struct window_features get_window_features_for_style(DWORD style, DWORD ex_style);
extern int get_widget_id( HWND hwnd );
extern HWND get_capture_window(void);
extern void init_monitors( int width, int height );
extern void set_screen_dpi( DWORD dpi );
extern RECT get_desktop_rect(void);

enum keyboard_lock_state{
  KEYBOARD_CAPS_LOCK = 0x01,
  KEYBOARD_NUM_LOCK = 0x02,
  KEYBOARD_SCROLL_LOCK = 0x04,
};
extern WORD vkey_to_scancode[];
extern UINT keycode_to_vkey( int keycode );
extern void send_keyboard_input( HWND hwnd, WORD vkey, WORD scan, DWORD flags );
extern void update_keyboard_lock_state( WORD vkey, UINT state );
extern void sync_modifier_key_state( WORD vkey, uint64_t modifier_key_states );
extern void distribute_input_event_callback( const input_event* event );

enum event_type
{
    DESKTOP_CHANGED,
    CONFIG_CHANGED,
    SURFACE_CHANGED,
    MOTION_EVENT,
    KEYBOARD_EVENT,
    IME_EVENT,
    SCREENLOCK_EVENT,
    WINDOW_FOCUS_EVENT,
    OVERLAY_DIRTY_EVENT,   /* new content on a child's shared surface: wake the owner to composite (driver-internal) */
    TRAY_ICON_CLICK_EVENT, /* tray icon clicked: restore this process's minimized windows */
};

struct ime_data
{
    ime_event_type ime_type;
    int input_text_len;
    char input_text[256];
    int delete_text_len;
    int move_direction;
    int enter_key_type;
};

extern pthread_mutex_t ime_mutex;
extern void OHOS_NotifyIMEStatus( HWND hwnd, UINT status );
extern BOOL OHOS_SetIMECompositionRect( HWND hwnd, RECT rect );
extern void send_ime_event( const ime_event* event );
extern void ime_attach(void);
extern void distribute_ime_callback( const struct ime_data* data );
extern void ime_on_screen_unlock(void);

/* screenlock */
extern void send_screenlock_event( const char* event_type );
extern void distribute_screenlock_callback( int unlocked );

union event_data
{
    enum event_type type;
    struct
    {
        enum event_type type;
        unsigned int    width;
        unsigned int    height;
    } desktop;
    struct
    {
        enum event_type type;
        unsigned int    dpi;
    } cfg;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        int             left;
        int             top;
        unsigned int    width;
        unsigned int    height;
        int             reason;
    } surface;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        INPUT           input;
    } motion;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        UINT            lock_state;
        uint64_t        modifier_key_states;
        INPUT           input;
    } kbd;
    struct
    {
        enum event_type type;
        struct ime_data data;
    } ime;
    struct
    {
        enum event_type type;
        int unlocked;
    } screenlock;
    struct
    {
        enum event_type type;
        HWND            hwnd;
        BOOL            gained;
    } focus;
    struct
    {
        enum event_type type;
        int             parent_widget_id;
    } overlay_dirty;
};

int send_event( const union event_data *data );

/* string helpers */

static inline void ascii_to_unicode( WCHAR *dst, const char *src, size_t len )
{
    while (len--) *dst++ = (unsigned char)*src++;
}

#endif  /* __WINE_OHOS_H */
