#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdarg.h>
#include <string.h>
#include <dlfcn.h>
#include <link.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winreg.h"
#include "ohos.h"
#include "surface_lut.h"
#include "wine/server.h"
#include "wine/debug.h"

#include "ohos_adapter.h"

WINE_DEFAULT_DEBUG_CHANNEL(ohos);

unsigned int screen_width = 0;
unsigned int screen_height = 0;
RECT virtual_screen_rect = { 0, 0, 0, 0 };
BOOL in_child_process = FALSE;  /* this parameter is deprecated */

static const unsigned int screen_bpp = 32;  /* we don't support other modes */

static RECT monitor_rc_work;
static int device_init_done;

typedef struct
{
    struct gdi_physdev dev;
} OHOS_PDEVICE;

static inline OHOS_PDEVICE *get_ohosdrv_dev(PHYSDEV dev)
{
    return (OHOS_PDEVICE*)dev;
}

static const struct user_driver_funcs ohos_drv_funcs;

/******************************************************************************
 *           init_monitors
 */
void init_monitors( int width, int height )
{
    static const WCHAR trayW[] = {'S','h','e','l','l','_','T','r','a','y','W','n','d',0};
    UNICODE_STRING name;
    RECT rect;
    HWND hwnd;

    RtlInitUnicodeString( &name, trayW );
    hwnd = NtUserFindWindowEx( 0, 0, &name, NULL, 0 );

    virtual_screen_rect.right = width;
    virtual_screen_rect.bottom = height;
    monitor_rc_work = virtual_screen_rect;

    if (!hwnd || !NtUserIsWindowVisible( hwnd )) return;
    if (!NtUserGetWindowRect( hwnd, &rect, NtUserGetWinMonitorDpi( hwnd, MDT_RAW_DPI ) )) return;
    if (rect.top) monitor_rc_work.bottom = rect.top;
    else monitor_rc_work.top = rect.bottom;
    TRACE( "found tray %p %s work area %s\n", hwnd,
           wine_dbgstr_rect( &rect ), wine_dbgstr_rect( &monitor_rc_work ));
}

/* wrapper for NtCreateKey that creates the key recursively if necessary */
static HKEY reg_create_key( const WCHAR *name, ULONG name_len )
{
    UNICODE_STRING nameW = { name_len, name_len, (WCHAR *)name };
    OBJECT_ATTRIBUTES attr;
    NTSTATUS status;
    HANDLE ret;

    attr.Length = sizeof(attr);
    attr.RootDirectory = 0;
    attr.ObjectName = &nameW;
    attr.Attributes = 0;
    attr.SecurityDescriptor = NULL;
    attr.SecurityQualityOfService = NULL;

    status = NtCreateKey( &ret, MAXIMUM_ALLOWED, &attr, 0, NULL, 0, NULL );
    if (status == STATUS_OBJECT_NAME_NOT_FOUND)
    {
        static const WCHAR registry_rootW[] = { '\\','R','e','g','i','s','t','r','y','\\' };
        DWORD pos = 0, i = 0, len = name_len / sizeof(WCHAR);

        /* don't try to create registry root */
        if (len > ARRAY_SIZE(registry_rootW) &&
            !memcmp( name, registry_rootW, sizeof(registry_rootW) ))
            i += ARRAY_SIZE(registry_rootW);

        while (i < len && name[i] != '\\') i++;
        if (i == len) return 0;
        for (;;)
        {
            nameW.Buffer = (WCHAR *)name + pos;
            nameW.Length = (i - pos) * sizeof(WCHAR);
            status = NtCreateKey( &ret, MAXIMUM_ALLOWED, &attr, 0, NULL, 0, NULL );

            if (attr.RootDirectory) NtClose( attr.RootDirectory );
            if (status) return 0;
            if (i == len) break;
            attr.RootDirectory = ret;
            while (i < len && name[i] == '\\') i++;
            pos = i;
            while (i < len && name[i] != '\\') i++;
        }
    }
    return ret;
}

/******************************************************************************
 *           set_screen_dpi
 */
void set_screen_dpi( DWORD dpi )
{
    static const WCHAR dpi_value_name[] = {'L','o','g','P','i','x','e','l','s',0};
    static const WCHAR dpi_key_name[] =
    {
        '\\','R','e','g','i','s','t','r','y',
        '\\','M','a','c','h','i','n','e',
        '\\','S','y','s','t','e','m',
        '\\','C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t',
        '\\','H','a','r','d','w','a','r','e',' ','P','r','o','f','i','l','e','s',
        '\\','C','u','r','r','e','n','t',
        '\\','S','o','f','t','w','a','r','e',
        '\\','F','o','n','t','s'
    };
    HKEY hkey;

    if ((hkey = reg_create_key( dpi_key_name, sizeof(dpi_key_name ))))
    {
        UNICODE_STRING name;
        RtlInitUnicodeString( &name, dpi_value_name );
        NtSetValueKey( hkey, &name, 0, REG_DWORD, &dpi, sizeof(dpi) );
        NtClose( hkey );
    }
}

/**********************************************************************
 *           fetch_display_metrics
 */
static void fetch_display_metrics(void)
{
    SERVER_START_REQ( get_window_rectangles )
    {
        req->handle = wine_server_user_handle( NtUserGetDesktopWindow() );
        req->relative = COORDS_CLIENT;
        if (!wine_server_call( req ))
        {
            screen_width  = reply->window.right;
            screen_height = reply->window.bottom;
        }
    }
    SERVER_END_REQ;

    if (!screen_width || !screen_height)  /* not initialized yet */
    {
        RECT desktop_rect = get_desktop_rect();
        screen_width  = desktop_rect.right - desktop_rect.left;
        screen_height = desktop_rect.bottom - desktop_rect.top;
    }

    init_monitors( screen_width, screen_height );
    TRACE( "screen %ux%u\n", screen_width, screen_height );
}

/**********************************************************************
 *           device_init
 *
 * Perform initializations needed upon creation of the first device.
 */
static void device_init(void)
{
    device_init_done = TRUE;
    fetch_display_metrics();
}

/******************************************************************************
 *           create_ohos_physdev
 */
static OHOS_PDEVICE *create_ohos_physdev(void)
{
    OHOS_PDEVICE *physdev;

    if (!device_init_done) device_init();

    if (!(physdev = calloc( 1, sizeof(*physdev) ))) return NULL;
    return physdev;
}

/**********************************************************************
 *           OHOS_CreateDC
 */
static BOOL OHOS_CreateDC( PHYSDEV *pdev, LPCWSTR device, LPCWSTR output, const DEVMODEW *initData )
{
    OHOS_PDEVICE *physdev = create_ohos_physdev();

    if (!physdev)
    {
        ERR("create dc failed.\n");
        return FALSE;
    }

    push_dc_driver( pdev, &physdev->dev, &ohos_drv_funcs.dc_funcs );
    return TRUE;
}

/**********************************************************************
 *           OHOS_CreateCompatibleDC
 */
static BOOL OHOS_CreateCompatibleDC( PHYSDEV orig, PHYSDEV *pdev )
{
    OHOS_PDEVICE *physdev = create_ohos_physdev();

    if (!physdev)
    {
        ERR("create compatible dc failed.\n");
        return FALSE;
    }

    push_dc_driver( pdev, &physdev->dev, &ohos_drv_funcs.dc_funcs );
    return TRUE;
}

/**********************************************************************
 *           OHOS_DeleteDC
 */
static BOOL OHOS_DeleteDC( PHYSDEV dev )
{
    OHOS_PDEVICE *physDev = get_ohosdrv_dev(dev);

    free( physDev );
    return TRUE;
}

/***********************************************************************
 *           OHOS_ChangeDisplaySettings
 */
LONG OHOS_ChangeDisplaySettings( LPDEVMODEW displays, LPCWSTR primary_name, HWND hwnd, DWORD flags, LPVOID lpvoid )
{
    FIXME( "(%p,%s,%p,0x%08x,%p)\n", displays, debugstr_w(primary_name), hwnd, flags, lpvoid );
    return DISP_CHANGE_SUCCESSFUL;
}


/***********************************************************************
 *           OHOS_UpdateDisplayDevices
 */
UINT OHOS_UpdateDisplayDevices( const struct gdi_device_manager *device_manager, void *param )
{
    if (!device_init_done) device_init();

    static const DWORD source_flags = DISPLAY_DEVICE_ATTACHED_TO_DESKTOP | DISPLAY_DEVICE_PRIMARY_DEVICE | DISPLAY_DEVICE_VGA_COMPATIBLE;
    struct pci_id pci_id = {0};
    struct gdi_monitor gdi_monitor =
    {
        .rc_monitor = virtual_screen_rect,
        .rc_work = monitor_rc_work,
    };
    const DEVMODEW mode =
    {
        .dmSize = sizeof(mode),
        .dmFields = DM_DISPLAYORIENTATION | DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL |
                    DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY,
        .dmBitsPerPel = screen_bpp, .dmPelsWidth = screen_width, .dmPelsHeight = screen_height, .dmDisplayFrequency = 60,
    };
    UINT dpi = NtUserGetSystemDpiForProcess( NULL );
    DEVMODEW current = mode;

    device_manager->add_gpu( "Wine GPU", &pci_id, NULL, param );
    device_manager->add_source( "Default", source_flags, dpi, param );
    device_manager->add_monitor( &gdi_monitor, param );

    current.dmFields |= DM_POSITION;
    device_manager->add_modes( &current, 1, &mode, param );

    TRACE("dpi %d, virtual_screen_rect %s, monitor_rc_work %s\n", dpi, wine_dbgstr_rect( &virtual_screen_rect ),
          wine_dbgstr_rect( &monitor_rc_work ));
    return STATUS_SUCCESS;
}

static const struct user_driver_funcs ohos_drv_funcs =
{
    .dc_funcs.pCreateCompatibleDC = OHOS_CreateCompatibleDC,
    .dc_funcs.pCreateDC = OHOS_CreateDC,
    .dc_funcs.pDeleteDC = OHOS_DeleteDC,
    .dc_funcs.priority = GDI_PRIORITY_GRAPHICS_DRV,

    .pSetCursor = OHOS_SetCursor,
    .pChangeDisplaySettings = OHOS_ChangeDisplaySettings,
    .pUpdateDisplayDevices = OHOS_UpdateDisplayDevices,
    .pCreateWindow = OHOS_CreateWindow,
    .pDesktopWindowProc = OHOS_DesktopWindowProc,
    .pDestroyWindow = OHOS_DestroyWindow,
    .pProcessEvents = OHOS_ProcessEvents,
    .pSetCapture = OHOS_SetCapture,
    .pSetDesktopWindow = OHOS_SetDesktopWindow,
    .pSetParent = OHOS_SetParent,
    .pShowWindow = OHOS_ShowWindow,
    .pWindowMessage = OHOS_WindowMessage,
    .pWindowPosChanging = OHOS_WindowPosChanging,
    .pCreateWindowSurface = OHOS_CreateWindowSurface,
    .pWindowPosChanged = OHOS_WindowPosChanged,
    .pGetWindowStateUpdates = OHOS_GetWindowStateUpdates,
    .pOpenGLInit = OHOS_OpenGLInit,
    .pVulkanInit = OHOS_VulkanInit,
    .pClipboardWindowProc = OHOS_ClipboardWindowProc,
    .pUpdateClipboard = OHOS_UpdateClipboard,

    .pNotifyIMEStatus = OHOS_NotifyIMEStatus,
    .pSetIMECompositionRect = OHOS_SetIMECompositionRect,
};

static HRESULT ohos_init( void *arg )
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init( &attr );
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
    pthread_mutex_init( &win_data_mutex, &attr );
    pthread_mutex_init( &ime_mutex, &attr );
    pthread_mutexattr_destroy( &attr );

    init_ohos_pasteboard();

    init_alpha_blend_lut();

    if (IsMainProcess())  /* running under arkui */
    {
        TRACE( "ohos_init: running under arkui main process\n" );
    }
    else
    {
        WARN( "ohos_init: running arkui isolate process\n" );
    }

    __wine_set_user_driver( &ohos_drv_funcs, WINE_GDI_DRIVER_VERSION );

    RegisterInputEventCallBack(distribute_input_event_callback);
    RegisterWindowEventCallBack(distribute_input_event_callback);
    RegisterIMEEventCallBack(send_ime_event);
    StartListeningForScreenlock(send_screenlock_event);
    /* cross-process GDI overlay compositing callback (owner side: device.c) */
    wineohos_register_gdi_overlay();

    return STATUS_SUCCESS;
}

const unixlib_entry_t __wine_unix_call_funcs[] =
{
    ohos_init,
};

C_ASSERT( ARRAYSIZE(__wine_unix_call_funcs) == unix_funcs_count );

#ifdef _WIN64

const unixlib_entry_t __wine_unix_call_wow64_funcs[] =
{
    ohos_init,
};

C_ASSERT( ARRAYSIZE(__wine_unix_call_wow64_funcs) == unix_funcs_count );

#endif /* _WIN64 */

