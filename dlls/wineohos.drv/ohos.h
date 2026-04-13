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

/* Define to the soname of the libGLESv3 library. */
#define SONAME_LIBGLESV3 "libGLESv3.so"

/* ohos private window data */
struct ohos_win_data
{
    HWND                hwnd;          /* hwnd that this private data belongs to */
    HWND                parent;        /* parent hwnd for child windows */
    struct window_rects rects;         /* window rects in monitor DPI, relative to parent client area */
    ohos_window         window;        /* native window wrapper that forwards calls to the desktop process */
    unsigned int        on_screen;     /* is window ordered in? (minimized or not) */
    unsigned int        shaped;        /* is window using a custom region shape? */
    unsigned int        layered;       /* is window layered and with valid attributes? */
    unsigned int        minimized;     /* is window minimized? */
    unsigned int        lazy_created;
};

struct ohos_client_surface
{
    struct client_surface client;
    HWND                  hwnd;
    ohos_window           window;
    RECT                  rect;
};

extern struct ohos_win_data *alloc_win_data( HWND hwnd );
extern void free_win_data( struct ohos_win_data *data );
extern struct ohos_win_data *get_win_data( HWND hwnd );
extern void release_win_data( struct ohos_win_data *data );
extern struct ohos_client_surface *ohos_client_surface_create( HWND hwnd, BOOL force );
extern UINT OHOS_OpenGLInit( UINT version, const struct opengl_funcs *opengl_funcs, const struct opengl_driver_funcs **driver_funcs );

extern void create_native_window( struct ohos_win_data *data );
extern void destroy_native_window( HWND hwnd );
extern ohos_window create_client_window( HWND hwnd );
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
extern int perform( HWND hwnd, int operation, ... );

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
extern void OHOS_UpdateClipboard(void);
extern LRESULT OHOS_ClipboardWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
extern BOOL OHOS_WindowPosChanging( HWND hwnd, UINT swp_flags, BOOL shaped, const struct window_rects *rects );
extern BOOL OHOS_CreateWindowSurface( HWND hwnd, BOOL layered, const RECT *surface_rect, struct window_surface **surface );
extern void OHOS_WindowPosChanged( HWND hwnd, HWND insert_after, HWND owner_hint, UINT swp_flags, BOOL fullscreen,
                                      const struct window_rects *new_rects, struct window_surface *surface );

extern unsigned int screen_width;
extern unsigned int screen_height;
extern RECT virtual_screen_rect;
extern MONITORINFOEXW default_monitor;

enum ohos_window_messages
{
    WM_OHOS_REFRESH = WM_WINE_FIRST_DRIVER_MSG,
};

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
extern const UINT keycode_to_vkey( int keycode );
extern void send_keyboard_input( HWND hwnd, WORD vkey, WORD scan, DWORD flags );
extern void update_keyboard_lock_state( WORD vkey, UINT state );
extern void distribute_input_event_callback( const input_event* event );

enum event_type
{
    DESKTOP_CHANGED,
    CONFIG_CHANGED,
    SURFACE_CHANGED,
    MOTION_EVENT,
    KEYBOARD_EVENT,
    IME_EVENT,
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
extern void ime_attach();
extern void distribute_ime_callback( const struct ime_data* data );

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
        ohos_window     window;
        BOOL            client;
        unsigned int    width;
        unsigned int    height;
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
        INPUT           input;
    } kbd;
    struct
    {
        enum event_type type;
        struct ime_data data;
    } ime;
};

int send_event( const union event_data *data );

/* string helpers */

static inline void ascii_to_unicode( WCHAR *dst, const char *src, size_t len )
{
    while (len--) *dst++ = (unsigned char)*src++;
}

#endif  /* __WINE_OHOS_H */
