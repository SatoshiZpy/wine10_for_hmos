#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <ace/xcomponent/native_xcomponent_key_event.h>

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

/* window property holding MAKELONGLONG(widget_id, owner_pid) on every
 * toplevel; readable from other wine processes through the wineserver */
const WCHAR ohos_window_id_prop[] =
    {'_','_','w','i','n','e','_','o','h','o','s','_','w','i','d','g','e','t',0};

#define NB_CACHED_BUFFERS 4
#define HAS_FLAGS(flags, required) (((flags) & (required)) == (required))

struct native_win_data
{
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
    /* Cross-process GDI sharing: the pixels live in an ashmem buffer shared
     * with the page owner process; in this mode no XComponent is created
     * and there is no local producer. */
    void                   *shared_pixels;      /* mmap of the shared buffer, NULL when local */
    int                     shared_stride;      /* bytes per row */
    int                     shared_width;
    int                     shared_height;
    int                     shared_id;          /* id assigned by the owner process */
    int                     shared_scale_num;   /* logical -> raw scale (num) */
    int                     shared_scale_den;   /* logical -> raw scale (den) */
    struct native_buffer    shared_buffer;      /* virtual buffer handed out by LOCK */
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

/* Resolve the native window to act on: shared cross-process windows have
 * none (the owner composites their pixels); everything else uses its own
 * XComponent window. */
static ohos_native_window get_window_handle( struct native_win_data *data )
{
    if (data->shared_pixels) return NULL;
    /* GDI flush: never block -- invisible windows (e.g. popups being created
     * before Show) have no surface yet and waiting would stall every flush
     * for the adapter's full surface timeout. */
    return GetNativeWindow( data->widget_id, FALSE );
}

static int get_win_parent( HWND parent )
{
    if (parent != NtUserGetDesktopWindow() && !NtUserGetAncestor( parent, GA_PARENT ))
        return HandleToLong( HWND_MESSAGE );
    return HandleToLong( parent );
}

void unmap_native_buffer( struct native_buffer *buffer )
{
    if (buffer->mapped_addr)
    {
        munmap( buffer->mapped_addr, buffer->mapped_size );
        buffer->mapped_addr = NULL;
        buffer->mapped_fd = -1;
        buffer->mapped_size = 0;
    }
}

int map_native_buffer( struct native_buffer *buffer )
{
    ohos_buffer_handle handle = buffer->buffer_handle;
    if (!handle) return STATUS_INVALID_HANDLE;

    /* Cache hit: same fd and same size, reuse existing mapping */
    if (buffer->mapped_addr && buffer->mapped_fd == handle->fd &&
        buffer->mapped_size == (size_t)handle->size)
        return STATUS_SUCCESS;  /* cache hit */

    /* Size changed: unmap the old mapping first */
    if (buffer->mapped_addr) unmap_native_buffer( buffer );

    buffer->mapped_addr = mmap( handle->virAddr, handle->size, PROT_READ | PROT_WRITE,
                                MAP_SHARED, handle->fd, 0 );
    if (buffer->mapped_addr == MAP_FAILED)
    {
        ERR( "mmap buffer handle failed\n" );
        buffer->mapped_addr = NULL;
        buffer->mapped_fd = -1;
        buffer->mapped_size = 0;
        return STATUS_MEMORY_NOT_ALLOCATED;
    }
    buffer->mapped_fd = handle->fd;
    buffer->mapped_size = handle->size;
    return STATUS_SUCCESS;
}

static void release_native_window( struct native_win_data *data )
{
    unsigned int i;

    for (i = 0; i < NB_CACHED_BUFFERS; i++)
    {
        if (data->buffers[i])
        {
            unmap_native_buffer( data->buffers[i] );
            OH_NativeWindow_NativeObjectUnreference(data->buffers[i]->win_buffer);
            free(data->buffers[i]);
            data->buffers[i] = NULL;
        }
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
    if ((owner_win = get_native_win_data( owner )))
        widget_id = owner_win->widget_id;

    /* Release the lock before GetFocusWindow(): it is a synchronous
     * round-trip to the ArkUI main thread (adapter JS invoke); holding
     * win_data_mutex across it deadlocks if the main thread is stuck in a
     * wine callback (mouse/refresh events call get_win_data). */
    release_win_data( data );

    if (widget_id <= 0)
    {
        WARN( "failed to get owner window, using current focus window instead, win %p\n", hwnd );
        widget_id = GetFocusWindow();
    }
    TRACE( "get owner window %d, win %p\n", widget_id, hwnd );
    return widget_id;
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

        struct native_buffer *old_buffer = win->buffers[i];
        if (old_buffer)
        {
            TRACE( "hwnd %p %d evicting buffer %p id %d from cache\n",
                win->hwnd, win->widget_id, old_buffer, i );
            unmap_native_buffer( old_buffer );
            OH_NativeWindow_NativeObjectUnreference(old_buffer->win_buffer);
            free(old_buffer);
            win->buffers[i] = NULL;
            win->fence_id[i] = -1;
        }
    }

    win->buffers[i] = buffer;
    win->fence_id[i] = buffer->fence_id;
    OH_NativeWindow_NativeObjectReference(buffer->win_buffer);
    TRACE( "hwnd %p %d %p -> %d\n", win->hwnd, win->widget_id, buffer, i );

done:
    insert_buffer_lru( win, i );
    return i;
}

static struct native_buffer *get_registered_buffer( struct native_win_data *win, int id )
{
    if (id < 0 || id >= NB_CACHED_BUFFERS || !win->buffers[id])
    {
        ERR( "unknown buffer %d for %p %d\n", id, win->hwnd, win->widget_id );
        return NULL;
    }
    return win->buffers[id];
}

static int dequeueBuffer( HWND hwnd, struct native_buffer **buffer );
static int dequeue_buffer_locked( HWND hwnd, struct native_win_data *win_data,
                                  struct native_buffer **buffer );
static int queue_buffer_locked( HWND hwnd, struct native_win_data *win_data,
                                struct native_buffer *locked_buffer,
                                const RECT *dirty_rect );
static void blit_overlays_into( struct native_win_data *win, struct native_buffer *buf );
static void gdi_lock(void);
static void gdi_unlock(void);

static int dequeueBuffer( HWND hwnd, struct native_buffer **buffer )
{
    ohos_native_window win;
    struct native_win_data *win_data;
    NTSTATUS status;

    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;
    gdi_lock();
    status = dequeue_buffer_locked( hwnd, win_data, buffer );
    gdi_unlock();
    return status;
}

/* caller holds gdi_cs */
static int dequeue_buffer_locked( HWND hwnd, struct native_win_data *win_data,
                                  struct native_buffer **buffer )
{
    ohos_native_window win;

    /* shared cross-process window: the virtual buffer maps the ashmem
     * pixels directly, no NativeWindow involved */
    if (win_data->shared_pixels)
    {
        struct native_buffer *buf = &win_data->shared_buffer;
        buf->width = win_data->shared_width;
        buf->height = win_data->shared_height;
        buf->stride = win_data->shared_stride / 4;
        buf->mapped_addr = win_data->shared_pixels;
        buf->mapped_fd = -1;
        buf->mapped_size = 0; /* not owned by the buffer (detach unmaps) */
        buf->buffer_id = -1;
        buf->win_buffer = NULL;
        buf->buffer_handle = NULL;
        *buffer = buf;
        return STATUS_SUCCESS;
    }

    if (!(win = get_window_handle( win_data ))) return STATUS_DEVICE_NOT_READY;

    ohos_window_buffer win_buffer;
    int fence = -1, ret;
    ret = OH_NativeWindow_NativeWindowRequestBuffer(win, &win_buffer, &fence);
    if (ret != 0) {
        ERR("request buffer failed, ret: %d\n", ret);
        return STATUS_BUFFER_OVERFLOW;
    }
    TRACE( "request buffer %p fence %d\n", win_buffer, fence);

    /* Wait for fence to ensure buffer is ready for writing.  Short timeout:
     * during window state transitions (maximize/restore animations) the
     * compositor may not return the buffer for a long time and the fence
     * never fires -- a 3s wait would wedge the window thread (two waits is
     * already 6s, visible as content lagging behind the transition); after
     * the timeout write anyway (this is a degraded path to begin with). */
    if (fence >= 0)
    {
        struct pollfd pfd = { .fd = fence, .events = POLLIN };
        int wait_ret = poll( &pfd, 1, 100 );
        if (wait_ret <= 0)
            WARN( "fence wait %s for fd %d\n", wait_ret == 0 ? "timeout" : "failed", fence );
        close( fence );
    }

    ohos_buffer_handle buffer_handle =
        OH_NativeWindow_GetBufferHandleFromNative(win_buffer);
    if (!buffer_handle)
    {
        ERR("get buffer_handle failed\n");
        return STATUS_BUFFER_OVERFLOW;
    }

    /* Check if this win_buffer is already cached with a persistent mapping.
     * OHOS uses triple-buffering; the same physical buffer is frequently
     * recycled.  By reusing the cached native_buffer (with its mmap),
     * we avoid redundant calloc/mmap/munmap/free per flush cycle. */
    unsigned int i;
    for (i = 0; i < NB_CACHED_BUFFERS; i++)
    {
        if (win_data->buffers[i] && win_data->buffers[i]->win_buffer == win_buffer)
        {
            struct native_buffer *buf = win_data->buffers[i];
            /* Reuse cached buffer: update metadata that may have changed */
            buf->buffer_handle = buffer_handle;
            buf->width  = buffer_handle->width;
            buf->height = buffer_handle->height;
            buf->stride = buffer_handle->stride / sizeof(DWORD);
            /* Remap if fd or size changed (e.g. after SET_BUFFER_GEOMETRY).
             * map_native_buffer detects stale mappings by comparing fd
             * and size, and automatically unmaps/remaps when needed. */
            map_native_buffer( buf );
            insert_buffer_lru( win_data, i );
            *buffer = buf;
            TRACE( "hwnd %p reused cached buffer %p id %d, size %d,%d\n",
                hwnd, buf, i, buf->width, buf->height);
            return STATUS_SUCCESS;
        }
    }

    /* New buffer: allocate, mmap, and register */
    struct native_buffer *buf = calloc( 1, sizeof(*buf) );

    buf->win_buffer = win_buffer;
    buf->buffer_handle = buffer_handle;
    buf->fence_id = -1;  /* fence already waited and closed */
    buf->mapped_fd = -1;
    buf->width  = buffer_handle->width;
    buf->height = buffer_handle->height;
    buf->stride = buffer_handle->stride / sizeof(DWORD);

    /* Persistent mmap: map the DMA buffer once and cache the address.
     * map_native_buffer caches the mapping; it remains valid after fd
     * close and across FlushBuffer calls as long as the same physical
     * buffer is reused via the LRU cache.  When the buffer is evicted
     * or the window is destroyed, unmap_native_buffer is called from
     * register_buffer() / release_native_window(). */
    map_native_buffer( buf );

    int buffer_id = register_buffer( win_data, buf );
    buf->buffer_id = buffer_id;
    *buffer = buf;
    TRACE( "hwnd %p got buffer %p fence %d buffer_id %d, size %d,%d\n",
        hwnd, buffer, fence, buffer_id, buf->width, buf->height);

    return STATUS_SUCCESS;
}

int cancelBuffer( HWND hwnd, int buffer_id )
{
    ohos_native_window win;
    struct native_buffer *buffer;
    struct native_win_data *win_data;
    int ret;

    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;

    /* shared window: nothing queued, nothing to abort */
    if (win_data->shared_pixels) return STATUS_SUCCESS;

    gdi_lock();

    if (!(buffer = get_registered_buffer( win_data, buffer_id )))
    {
        gdi_unlock();
        return STATUS_INVALID_HANDLE;
    }

    if (!(win = get_window_handle( win_data )))
    {
        ERR("hwnd %p: failed to get native window for cancelBuffer\n", hwnd);
        gdi_unlock();
        return STATUS_DEVICE_NOT_READY;
    }

    TRACE( "hwnd %p buffer %p id %d\n", hwnd, buffer, buffer_id );

    /* AbortBuffer returns the buffer to the NativeWindow pool without
     * submitting it to the compositor.  This is the correct way to cancel
     * a dequeued buffer NativeObjectUnreference only decrements the
     * refcount but does NOT return the buffer to the queue, causing
     * buffer pool leaks and rendering failures (yellow content). */
    ret = OH_NativeWindow_NativeWindowAbortBuffer(win, buffer->win_buffer);
    if (ret != 0)
    {
        ERR("AbortBuffer error: %d\n", ret);
    }

    /* Clear locked_buffer so a subsequent UNLOCK_AND_POST won't try to
     * queue a buffer that has already been returned to the pool. */
    if (win_data->locked_buffer == buffer)
        win_data->locked_buffer = NULL;

    gdi_unlock();
    return STATUS_SUCCESS;
}

static int queueBuffer( HWND hwnd, struct native_buffer *locked_buffer, const RECT *dirty_rect )
{
    struct native_win_data *win_data;
    int ret;

    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;
    gdi_lock();
    ret = queue_buffer_locked( hwnd, win_data, locked_buffer, dirty_rect );
    gdi_unlock();
    return ret;
}

/* caller holds gdi_cs */
static int queue_buffer_locked( HWND hwnd, struct native_win_data *win_data,
                                struct native_buffer *locked_buffer,
                                const RECT *dirty_rect )
{
    ohos_native_window win;
    struct native_buffer *buffer;
    int ret;

    /* shared cross-process window: notify the owner that pixels changed;
     * the RPC is synchronous, i.e. returns after the owner composited */
    if (win_data->shared_pixels)
    {
        int x = 0, y = 0, w = win_data->shared_width, h = win_data->shared_height;
        if (dirty_rect && !IsRectEmpty( dirty_rect ))
        {
            x = dirty_rect->left;
            y = dirty_rect->top;
            w = dirty_rect->right - dirty_rect->left;
            h = dirty_rect->bottom - dirty_rect->top;
        }
        TRACE( "hwnd %p shared flush id %d rect (%d,%d)-(%d,%d)\n",
               hwnd, win_data->shared_id, x, y, x + w, y + h );
        return RemoteGdiFlush( win_data->shared_id, x, y, w, h );
    }

    if (!(win = get_window_handle( win_data ))) return STATUS_DEVICE_NOT_READY;

    if (!(buffer = get_registered_buffer( win_data, locked_buffer->buffer_id ))) return STATUS_INVALID_HANDLE;
    if (buffer != locked_buffer) return STATUS_INVALID_HANDLE;

    TRACE( "hwnd %p buffer %p fence %d buffer_id %d, size %d,%d\n",
           hwnd, buffer, buffer->fence_id, buffer->buffer_id, buffer->width, buffer->height);
    Region region;
    struct Rect rects[1];
    if (dirty_rect && !IsRectEmpty( dirty_rect ))
    {
        rects[0].x = dirty_rect->left;
        rects[0].y = dirty_rect->top;
        rects[0].w = dirty_rect->right - dirty_rect->left;
        rects[0].h = dirty_rect->bottom - dirty_rect->top;
        region.rects = rects;
        region.rectNumber = 1;
    }
    else
    {
        region.rects = NULL;
        region.rectNumber = 0;
    }

    /* Blend the pixels of the foreign child windows on this page (the page
     * owner process is the compositor) and submit them with this frame. */
    blit_overlays_into( win_data, buffer );

    ret = OH_NativeWindow_NativeWindowFlushBuffer(win, buffer->win_buffer, buffer->fence_id, region);
    if (ret != 0) {
        ERR("Flush buffer error: %d", ret);
    }
    return STATUS_SUCCESS;
}

static const char *get_operation(int operation)
{
    switch (operation)
    {
        case SET_BUFFER_GEOMETRY:      return "SET_BUFFER_GEOMETRY";
        case GET_BUFFER_GEOMETRY:      return "GET_BUFFER_GEOMETRY";
        case GET_FORMAT:               return "GET_FORMAT";
        case SET_FORMAT:               return "SET_FORMAT";
        case SET_SWAP_INTERVAL:        return "SET_SWAP_INTERVAL";
        case GET_SWAP_INTERVAL:        return "GET_SWAP_INTERVAL";
        case NATIVE_WINDOW_LOCK:       return "NATIVE_WINDOW_LOCK";
        case NATIVE_WINDOW_UNLOCK_AND_POST: return "NATIVE_WINDOW_UNLOCK_AND_POST";
        default:                       return "UNKNOWN_OPERATION";
    }
}

BOOL is_tooltips_hwnd(HWND hwnd)
{
    static const WCHAR tooltips_classnameW[] = {
        't','o','o','l','t','i','p','s','_',
        'c','l','a','s','s','3','2',0 };
    static const WCHAR sap_tooltips_classnameW[] = {
        'S','A','P',' ','G','U','I',' ',
        'T','o','o','l','t','i','p',0 };
    WCHAR buffer[64];
    UNICODE_STRING name = {.Buffer = buffer, .MaximumLength = sizeof(buffer)};

    if (!NtUserGetClassName(hwnd, FALSE, &name)) return FALSE;
    TRACE("hwnd: %p, class name: %s\n", hwnd, debugstr_w(buffer));
    return !wcscmp(buffer, tooltips_classnameW) || !wcscmp(buffer, sap_tooltips_classnameW);
}

int perform( HWND hwnd, int operation, ... )
{
    int ret = STATUS_SUCCESS;
    ohos_native_window win;
    struct native_win_data *win_data;
    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;

    va_list args;
    gdi_lock();

    /* shared cross-process window: HandleOpt semantics are local; LOCK and
     * UNLOCK_AND_POST fall through (dequeue/queue branch internally) */
    if (win_data->shared_pixels &&
        operation != NATIVE_WINDOW_LOCK &&
        operation != NATIVE_WINDOW_UNLOCK_AND_POST)
    {
        RECT *bounds;
        va_start( args, operation );
        bounds = va_arg( args, RECT * );
        va_end( args );
        if (operation == GET_BUFFER_GEOMETRY && bounds)
        {
            bounds->left = 0;
            bounds->top = 0;
            bounds->right = win_data->shared_width;
            bounds->bottom = win_data->shared_height;
        }
        gdi_unlock();
        return STATUS_SUCCESS;
    }

    int args_list[4];
    memset( args_list, 0, sizeof(args_list) );
    va_start( args, operation );

    /* LOCK / UNLOCK_AND_POST fall through to the dequeue/queue paths below
     * (which carry the shared cross-process branch internally, with no local
     * NativeWindow) and must not be intercepted by get_window_handle here;
     * the other ops require a local window. */
    if (operation != NATIVE_WINDOW_LOCK &&
        operation != NATIVE_WINDOW_UNLOCK_AND_POST &&
        !(win = get_window_handle( win_data )))
    {
        va_end( args );
        gdi_unlock();
        return STATUS_DEVICE_NOT_READY;
    }

    switch (operation)
    {
    case SET_FORMAT:
        args_list[0] = va_arg( args, int );
        TRACE( "hwnd %p %s format %d\n", hwnd, get_operation(operation), args_list[0] );
        ret = OH_NativeWindow_NativeWindowHandleOpt(win, operation, args_list[0]);
        if (ret != 0) {
            ERR("set buffer format fail, ret=%d\n", ret);
        }
        if (!ret) win_data->buffer_format = args_list[0];
        break;
    case SET_SWAP_INTERVAL:
        args_list[0] = va_arg( args, int );
        TRACE( "hwnd %p %s interval %d\n", hwnd, get_operation(operation), args_list[0] );
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
        TRACE( "hwnd %p %s size %d-%d\n", hwnd, get_operation(operation), args_list[0], args_list[1] );
        ret = OH_NativeWindow_NativeWindowHandleOpt(win, operation, args_list[0], args_list[1]);
        if (ret != 0) {
            ERR("set buffer geometry fail, ret=%d\n", ret);
        }
        break;
    }
    case GET_BUFFER_GEOMETRY:
    {
        RECT *bounds = va_arg( args, RECT * );
        TRACE( "hwnd %p %s\n", hwnd, get_operation(operation));
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
        TRACE( "hwnd %p %s size %d-%d\n", hwnd, get_operation(operation), width_dst, height_dst );
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
        TRACE( "hwnd %p %s bounds %s, ret %d\n", hwnd, get_operation(operation), wine_dbgstr_rect(bounds), ret );
        break;
    }
    case NATIVE_WINDOW_UNLOCK_AND_POST:
    {
        const RECT *dirty_rect = va_arg( args, const RECT * );
        TRACE( "hwnd %p %s dirty %s\n", hwnd, get_operation(operation), wine_dbgstr_rect(dirty_rect) );
        ret = STATUS_BUFFER_OVERFLOW;
        if (win_data->locked_buffer)
        {
            ret = queueBuffer( hwnd, win_data->locked_buffer, dirty_rect );
            win_data->locked_buffer = NULL;
        }
        TRACE( "hwnd %p %s, ret %d\n", hwnd, get_operation(operation), ret );
        break;
    }
    default:
        FIXME( "unsupported perform hwnd %p op %d %s\n", hwnd, operation, get_operation(operation) );
        break;
    }
    va_end( args );
    gdi_unlock();
    return ret;
}

int set_swap_interval( HWND hwnd, int interval )
{
    ohos_native_window win;
    struct native_win_data *win_data;
    
    TRACE( "hwnd %p interval %d\n", hwnd, interval );

    if (!(win_data = get_native_win_data( hwnd ))) return STATUS_INVALID_HANDLE;
    win_data->swap_interval = interval;

    if (!(win = get_window_handle( win_data ))) return STATUS_SUCCESS;
    int ret = OH_NativeWindow_NativeWindowHandleOpt(win, SET_SWAP_INTERVAL, interval);
    if (ret != 0) {
        ERR("setSwapInterval_ioctl error: %d\n", ret);
    }
    return STATUS_SUCCESS;
}

static int get_pan_scroll_delta( float offset, float *last_offset, int *last_sign )
{
    float abs_offset, abs_last_offset;
    int sign, level, last_level;

    if (offset == 0.0f) return 0;

    sign = (offset > 0) - (offset < 0);
    if (*last_sign != 0 && sign != *last_sign)
    {
        *last_offset = 0.0f;
        *last_sign = 0;
    }
    *last_sign = sign;

    abs_offset = offset < 0 ? -offset : offset;
    abs_last_offset = *last_offset < 0 ? -*last_offset : *last_offset;
    level = (int)(abs_offset / WHEEL_DELTA);
    last_level = (int)(abs_last_offset / WHEEL_DELTA);

    if (level <= last_level)
    {
        if (level < last_level) *last_offset = offset;
        return 0;
    }

    *last_offset = sign * level * WHEEL_DELTA;
    return sign * (level - last_level) * WHEEL_DELTA;
}

void mouse_event_callback( const input_event* event )
{
    static float last_mouse_screen_x = 0;
    static float last_mouse_screen_y = 0;
    union event_data data;

    data.type = MOTION_EVENT;
    data.motion.hwnd = LongToHandle(event->hwnd);
    data.motion.input.type = INPUT_MOUSE;
    data.motion.input.mi.dx = event->data.mouse_event.screen_x;
    data.motion.input.mi.dy = event->data.mouse_event.screen_y;
    data.motion.input.mi.mouseData   = 0;
    data.motion.input.mi.time        = 0;
    data.motion.input.mi.dwExtraInfo = 0;
    data.motion.input.mi.dwFlags     = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
    int action = event->data.mouse_event.action;
    int button = event->data.mouse_event.button;
    switch (action)
    {
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
        {
            data.motion.input.mi.dx = last_mouse_screen_x;
            data.motion.input.mi.dy = last_mouse_screen_y;
            float wheel_x = event->data.mouse_event.wheel_x;
            float wheel_y = event->data.mouse_event.wheel_y;
            int source_tool = event->data.mouse_event.source_tool;

            if (source_tool == OHOS_SOURCE_TOOL_MOUSE)
            {
                /* Mouse wheel: each notch click is a discrete event, keep the
                 * original one-notch-per-event behavior. */
                if (wheel_y)
                {
                    data.motion.input.mi.mouseData = wheel_y < 0 ? -WHEEL_DELTA : WHEEL_DELTA;
                    data.motion.input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_WHEEL;
                    send_event(&data);
                }
                if (wheel_x)
                {
                    data.motion.input.mi.mouseData = wheel_x < 0 ? -WHEEL_DELTA : WHEEL_DELTA;
                    data.motion.input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_HWHEEL;
                    send_event(&data);
                }
            }
            else
            {
                /* Touchpad/touchscreen pan offsets are cumulative during a gesture.
                 * Convert each axis to wheel notches and emit only newly crossed
                 * notch levels. */
                static float pad_y_offset = 0.0f;
                static float pad_x_offset = 0.0f;
                static int pad_y_sign = 0;
                static int pad_x_sign = 0;
                int delta;

                if ((delta = get_pan_scroll_delta( wheel_y, &pad_y_offset, &pad_y_sign )))
                {
                    data.motion.input.mi.mouseData = delta;
                    data.motion.input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_WHEEL;
                    send_event(&data);
                }
                if ((delta = get_pan_scroll_delta( wheel_x, &pad_x_offset, &pad_x_sign )))
                {
                    data.motion.input.mi.mouseData = delta;
                    data.motion.input.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_HWHEEL;
                    send_event(&data);
                }
            }
            return;
        }
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
    data.kbd.hwnd = LongToHandle(event->hwnd);

    data.kbd.lock_state = 0;
    if (event->data.key_event.is_num_lockon) data.kbd.lock_state |= KEYBOARD_NUM_LOCK;
    if (event->data.key_event.is_caps_lockon) data.kbd.lock_state |= KEYBOARD_CAPS_LOCK;
    if (event->data.key_event.is_scroll_lockon) data.kbd.lock_state |= KEYBOARD_SCROLL_LOCK;
    data.kbd.modifier_key_states = event->data.key_event.modifier_key_states;

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
    switch (event->type)
    {
        case OHOS_MOUSE_EVENT:
            mouse_event_callback(event);
            break;
        case OHOS_KEY_EVENT:
            key_event_callback(event);
            break;

        case OHOS_WINDOW_RECT_CHANGE_EVENT:
        {
            HWND hwnd = LongToHandle(event->hwnd);
            if (!hwnd) {
                return;
            }

            int left = event->data.window_rect_change_event.left;
            int top = event->data.window_rect_change_event.top;
            unsigned int width = event->data.window_rect_change_event.width;
            unsigned int height = event->data.window_rect_change_event.height;
            int reason = event->data.window_rect_change_event.reason;

            /* Send SURFACE_CHANGED event to event queue */
            union event_data data;
            data.surface.type = SURFACE_CHANGED;
            data.surface.hwnd = hwnd;
            data.surface.left = left;
            data.surface.top = top;
            data.surface.width = width;
            data.surface.height = height;
            data.surface.reason = reason;
            send_event(&data);
            break;
        }
        case OHOS_WINDOW_FOCUS_EVENT:
        {
            HWND hwnd = LongToHandle(event->hwnd);
            if (!hwnd) {
                return;
            }
            union event_data data;
            data.type = WINDOW_FOCUS_EVENT;
            data.focus.hwnd = hwnd;
            data.focus.gained = TRUE;
            send_event(&data);
            break;
        }
        case OHOS_WINDOW_BLUR_EVENT:
        {
            HWND hwnd = LongToHandle(event->hwnd);
            if (!hwnd) {
                return;
            }
            union event_data data;
            data.type = WINDOW_FOCUS_EVENT;
            data.focus.hwnd = hwnd;
            data.focus.gained = FALSE;
            send_event(&data);
            break;
        }
        default:
            return;
    }
}

static int create_window( HWND hwnd, int window_type, const struct window_bounds *bounds )
{
    struct window_param param;
    struct native_win_data *win_data;
    if (!(win_data = create_native_win_data( hwnd )))
        return -1;

    TRACE( "hwnd %p window_type (%d) window bounds (%d,%d)-(%d,%d)\n", win_data->hwnd, window_type,
        bounds->left, bounds->top, bounds->width, bounds->height );

    param.container = ENTRY_ABILITY;
    param.type = window_type;
    param.hwnd = HandleToLong(hwnd);
    param.pid = getpid();
    param.bounds = *bounds;

    param.parent_id = (window_type != MAIN_WINDOW) ?
        get_owner_window_id(hwnd) : 0;

    param.features = get_window_features(hwnd);
    param.state = get_window_state(hwnd);

    win_data->widget_id = CreateNativeWindow(&param);
    if (win_data->widget_id <= 0) {
        ERR("create native window error\n");
        goto failed;
    }
    win_data->container = param.container;
    win_data->win_type = param.type;
    win_data->generation++;

    /* publish the native identity so foreign-process children can find the
     * host page for their client surfaces (widget and pid carried as a
     * window property) */
    NtUserSetProp( win_data->hwnd, ohos_window_id_prop,
                   (HANDLE)((ULONG_PTR)(UINT)win_data->widget_id |
                            ((ULONG_PTR)(UINT)getpid() << 32)) );

    TRACE( "hwnd %p widget_id %d\n", win_data->hwnd, win_data->widget_id );
    return win_data->widget_id;

failed:
    if (win_data) free_native_win_data(win_data);
    return -1;
}

/* Cross-process GDI sharing: a window embedded into a foreign toplevel keeps
 * its pixels in an ashmem buffer mapped and written by this process; the
 * page owner process composites them into its own GDI surface.  Neither side
 * creates an XComponent, and no local producer is involved. */

BOOL has_remote_gdi_producer( HWND hwnd )
{
    struct native_win_data *win_data = get_native_win_data( hwnd );
    return win_data && win_data->shared_pixels && win_data->shared_id > 0;
}

int host_gdi_widget_id( HWND hwnd )
{
    struct native_win_data *win_data = get_native_win_data( hwnd );
    if (!win_data || !win_data->shared_pixels) return 0;
    return win_data->shared_id;
}

int attach_remote_gdi_producer( HWND hwnd, void *pixels, int width,
                                int height, int stride, int shared_id,
                                int scale_num, int scale_den )
{
    struct native_win_data *win_data = get_native_win_data( hwnd );

    if (!win_data && !(win_data = create_native_win_data( hwnd ))) return -1;

    if (win_data->shared_pixels)
    {
        ERR( "hwnd %p: shared GDI buffer already attached\n", hwnd );
        return -1;
    }
    win_data->shared_pixels = pixels;
    win_data->shared_width = width;
    win_data->shared_height = height;
    win_data->shared_stride = stride;
    win_data->shared_id = shared_id;
    /* logical(96) -> raw pixel scale (the host-side compositing coordinate
     * system); position-only changes must use the same ratio when updating
     * the overlay offset */
    if (scale_num > 0 && scale_den > 0)
    {
        win_data->shared_scale_num = scale_num;
        win_data->shared_scale_den = scale_den;
    }
    else
    {
        win_data->shared_scale_num = win_data->shared_scale_den = 1;
    }
    win_data->widget_id = 0;
    memset( &win_data->shared_buffer, 0, sizeof(win_data->shared_buffer) );
    TRACE( "hwnd %p attached shared GDI surface id %d %dx%d scale %d/%d\n",
           hwnd, shared_id, width, height, win_data->shared_scale_num,
           win_data->shared_scale_den );
    return 0;
}

/* Fetch the shared surface id for lightweight bounds-update RPCs on
 * position-only changes. */
int get_remote_gdi_info( HWND hwnd, int *shared_id, int *scale_num, int *scale_den )
{
    struct native_win_data *win_data = get_native_win_data( hwnd );

    if (!win_data || !win_data->shared_pixels) return -1;
    if (shared_id) *shared_id = win_data->shared_id;
    if (scale_num) *scale_num = win_data->shared_scale_num;
    if (scale_den) *scale_den = win_data->shared_scale_den;
    return 0;
}

/* Scale-only query: the shared surface need not be attached (shared_scale_*
 * survives detach, so a deferred rebuild of a hidden window can avoid
 * degrading the scale to 1:1). */
int get_remote_gdi_scale( HWND hwnd, int *scale_num, int *scale_den )
{
    struct native_win_data *win_data = get_native_win_data( hwnd );

    if (!win_data) return -1;
    if (win_data->shared_scale_num <= 0 || win_data->shared_scale_den <= 0)
        return -1;
    if (scale_num) *scale_num = win_data->shared_scale_num;
    if (scale_den) *scale_den = win_data->shared_scale_den;
    return 0;
}

BOOL detach_remote_gdi_producer( HWND hwnd, int *shared_id )
{
    struct native_win_data *win_data = get_native_win_data( hwnd );

    if (!win_data || !win_data->shared_pixels) return FALSE;
    if (shared_id) *shared_id = win_data->shared_id;
    /* The pixels mapping and the shared memory are owned by the creating side
     * (this process's adapter): the unmap happens on the
     * DestroyRemoteGdiSharedSurface path; here we only disassociate. */
    win_data->shared_pixels = NULL;
    win_data->shared_id = 0;
    win_data->shared_width = win_data->shared_height = 0;
    win_data->shared_stride = 0;
    TRACE( "hwnd %p shared GDI surface detached\n", hwnd );
    return TRUE;
}

/* Owner-side compositing: blit the shared pixels of foreign child windows
 * into this process's GDI surface (this process acts as the compositor of
 * the page being embedded into). */

struct remote_gdi_overlay
{
    int                     id;
    int                     parent_widget_id;
    int                     off_x, off_y, w, h, stride;
    int                     visible;
    const void             *pixels;
    struct remote_gdi_overlay *next;
};

static struct remote_gdi_overlay *overlay_list;
static pthread_mutex_t gdi_cs;   /* recursive: no Win32 CS on the unix side */

static void gdi_lock(void)
{
    pthread_mutex_lock( &gdi_cs );
}

static void gdi_unlock(void)
{
    pthread_mutex_unlock( &gdi_cs );
}

/* Blend every foreign pixel plane attached to this page (widget_id) into the buffer. */
static void blit_overlays_into( struct native_win_data *win, struct native_buffer *buf )
{
    struct remote_gdi_overlay *ov;

    if (!buf || !buf->mapped_addr || !win->widget_id) return;
    for (ov = overlay_list; ov; ov = ov->next)
    {
        int x0, y0, x1, y1, y;
        if (ov->parent_widget_id != win->widget_id) continue;
        if (!ov->visible) continue;  /* hidden child (tab switch): skip in compositing */
        x0 = max( ov->off_x, 0 );
        y0 = max( ov->off_y, 0 );
        x1 = min( ov->off_x + ov->w, buf->width );
        y1 = min( ov->off_y + ov->h, buf->height );
        for (y = y0; y < y1; y++)
        {
            memcpy( (char *)buf->mapped_addr + (size_t)y * buf->stride * 4 + x0 * 4,
                    (const char *)ov->pixels + (size_t)(y - ov->off_y) * ov->stride + (x0 - ov->off_x) * 4,
                    (x1 - x0) * 4 );
        }
    }
}

/* adapter overlay callback (ADD/BLIT/REMOVE/UPDATE/VISIBLE), runs on the IPC listener thread */
static void remote_gdi_overlay_notify( int op, int id, int parent_widget_id,
                                       int off_x, int off_y, int width,
                                       int height, int stride,
                                       const void *pixels )
{
    struct remote_gdi_overlay **link, *ov;
    int wake_page = 0;

    gdi_lock();
    switch (op)
    {
    case REMOTE_GDI_OP_ADD:
        ov = calloc( 1, sizeof(*ov) );
        if (!ov) break;
        ov->id = id;
        ov->parent_widget_id = parent_widget_id;
        ov->off_x = off_x;
        ov->off_y = off_y;
        ov->w = width;
        ov->h = height;
        ov->stride = stride;
        ov->visible = 1;
        ov->pixels = pixels;
        ov->next = overlay_list;
        overlay_list = ov;
        /* This function runs on the adapter's IPC ListenLoop thread, which has
         * no wine TEB.  Wine's TRACE/WARN logging fetches the thread context
         * through NtCurrentTeb; on a non-wine thread that dereferences a
         * garbage pointer and SIGSEGVs (this once crashed the main process,
         * killed the host, and made cross-process embedding look like a hang).
         * Only fprintf is safe here. */
        fprintf( stderr, "ohos: overlay add id %d page %d %dx%d at (%d,%d)\n", id,
                 parent_widget_id, width, height, off_x, off_y );
        break;
    case REMOTE_GDI_OP_REMOVE:
        for (link = &overlay_list; *link; link = &(*link)->next)
        {
            if ((*link)->id == id)
            {
                ov = *link;
                *link = ov->next;
                free( ov );
                break;
            }
        }
        fprintf( stderr, "ohos: overlay remove id %d\n", id );
        break;
    case REMOTE_GDI_OP_UPDATE: /* position-only change, move the overlay offset (hot while dragging) */
        for (ov = overlay_list; ov; ov = ov->next)
        {
            if (ov->id != id) continue;
            ov->off_x = off_x;
            ov->off_y = off_y;
            wake_page = parent_widget_id;
            break;
        }
        break;
    case REMOTE_GDI_OP_VISIBLE: /* tab switching etc.  off_x carries 0/1.  The shared surface
              * is kept; a hidden child is skipped in compositing, and the child's
              * first BLIT once shown wakes the host again. */
        for (ov = overlay_list; ov; ov = ov->next)
        {
            if (ov->id != id) continue;
            ov->visible = off_x != 0;
            /* When hiding, the host area is refreshed by wine's own repaint flow
             * (parent invalidation -> the flush simply stops blending this
             * overlay); when showing, no wake is needed, wait for the child's
             * first frame. */
            break;
        }
        break;
    case REMOTE_GDI_OP_BLIT: /* the child's shared surface has a new frame.  Do not dequeue and
              * composite here -- the producer-side buffer we would get is a dirty
              * free buffer, and queueing it directly would overwrite the host's
              * current frame (white flash).  The host's own GDI flush
              * (queue_buffer_locked) already blends every overlay of the page.
              * In the steady state the host does not repaint on its own, so wake
              * it for one extra flush to blend the new frame in. */
        wake_page = parent_widget_id;
        break;
    default:
        break;
    }
    gdi_unlock();

    /* This callback runs on the IPC ListenLoop thread (no wine TEB) and must
     * not call NtUserPostMessage or other win32u entry points directly -- hand
     * the event to the desktop thread via send_event; process_events then wakes
     * the owner for the composite refresh on a proper wine thread. */
    if (wake_page > 0)
    {
        union event_data event;
        memset( &event, 0, sizeof(event) );
        event.overlay_dirty.type = OVERLAY_DIRTY_EVENT;
        event.overlay_dirty.parent_widget_id = wake_page;
        send_event( &event );
    }
}

/* Called from the PE-side init (unix ohos_init): registers the overlay
 * callback and initializes the lock */
void wineohos_register_gdi_overlay(void)
{
    static BOOL done;
    pthread_mutexattr_t attr;

    if (done) return;
    done = TRUE;
    pthread_mutexattr_init( &attr );
    pthread_mutexattr_settype( &attr, PTHREAD_MUTEX_RECURSIVE );
    pthread_mutex_init( &gdi_cs, &attr );
    pthread_mutexattr_destroy( &attr );
    RegisterRemoteGdiOverlayCallback( remote_gdi_overlay_notify );
    TRACE( "remote GDI overlay handler registered\n" );
}

void create_native_window( struct ohos_win_data *data ){
    DWORD style, ex_style;
    WCHAR text[1024];
    int window_type = POPUP_WINDOW;

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

    /* WS_EX_LAYERED|WS_EX_TRANSPARENT together mark a pure decoration window.
     * The OHOS compositor displays the subwindow buffer without per-pixel alpha blending, 
     * so such a "shadow" would cover the menu it decorates with solid color. 
     * These windows get no native window at all (widget_id = -1): OHOS_WindowPosChanging
     * then vetoes the driver surface and wine keeps them on the default surface,
     * where they stay invisible. Visual loss: the soft shadow only. */
    if ((ex_style & (WS_EX_LAYERED | WS_EX_TRANSPARENT)) == (WS_EX_LAYERED | WS_EX_TRANSPARENT))
    {
        data->widget_id = -1;
        return;
    }

    if ((style & (WS_POPUP | WS_CHILD)) == WS_CHILD)
        WARN("A top-level window that has the WS_CHILD style (which is usually invalid/inconsistent).\n");

    if ((style & WS_POPUP) || (ex_style & WS_EX_DLGMODALFRAME) || (ex_style & WS_EX_TOOLWINDOW))
        window_type = POPUP_WINDOW;
    else if ((style & WS_OVERLAPPEDWINDOW) == WS_OVERLAPPEDWINDOW || (style & WS_CAPTION) == WS_CAPTION)
        window_type = MAIN_WINDOW;
    else
        WARN("The popup window style is used by default.\n");

    if (is_tooltips_hwnd(data->hwnd))
        window_type = TOOLTIP;

    TRACE( "hwnd %p style %08x ex_style %08x\n", data->hwnd, style, ex_style );

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
    data->widget_id = create_window(data->hwnd, window_type, &bounds);
    if (data->widget_id <= 0) {
        return;
    }
    data->buffer_width = cx;
    data->buffer_height = cy;

    /* set the window text */
    if (!NtUserInternalGetWindowText(data->hwnd, text, ARRAY_SIZE(text))) text[0] = 0;
    set_window_title(data->hwnd, text);
}

void destroy_native_window( HWND hwnd )
{
    struct native_win_data *win_data;
    if (!(win_data = get_native_win_data( hwnd )))
        return;

    TRACE( "hwnd %p widget_id %d\n", hwnd, win_data->widget_id );

    NtUserRemoveProp( hwnd, ohos_window_id_prop );
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

    TRACE( "hwnd %p win_rects %s old_rects %s style %08x flags %08x\n",
           hwnd, debugstr_window_rects(rects), debugstr_window_rects(old_rects), style, flags );

    if (EqualRect(&rects->visible, &old_rects->visible) ||
          IsRectEmpty( &rects->window ))
        return STATUS_SUCCESS;

    if (HAS_FLAGS(flags, SWP_NOSIZE | SWP_NOMOVE))
    {
        TRACE( "window flags contains both SWP_NOSIZE and SWP_NOMOVE\n");
        return STATUS_SUCCESS;
    }

    struct window_bounds bounds = { rects->visible.left,
                                    rects->visible.top,
                                    rects->visible.right - rects->visible.left,
                                    rects->visible.bottom - rects->visible.top };
    SetWindowBounds(win_data->widget_id, &bounds);

    TRACE( "hwnd %p widget_id %d\n", hwnd, win_data->widget_id );
    return STATUS_SUCCESS;
}

int set_capture( HWND hwnd )
{
    if (hwnd && !get_native_win_data( hwnd )) return STATUS_INVALID_HANDLE;

    TRACE( "hwnd %p\n", hwnd );

    InterlockedExchangePointer( (void **)&capture_window, hwnd);
    return STATUS_SUCCESS;
}

int set_cursor( HWND hwnd, int id, int width, int height,
                int hotspotx, int hotspoty, const unsigned int *bits )
{
    struct native_win_data *data = get_native_win_data( hwnd );
    if (!data) return STATUS_INVALID_HANDLE;

    /*
     * Three cases:
     *   1) id > 0 && bits == NULL  -> the application requested a Windows system
     *      cursor that get_cursor_system_id() mapped to a HarmonyOS
     *      built-in pointer style. The wineohos.drv enum is 1-based so 0
     *      can stay the "not a system cursor" sentinel; subtract 1 here
     *      before forwarding to SetCursorStyle (which expects raw
     *      OhosCursorType values where kDefault = 0).
     *   2) id == 0 && bits == NULL -> the application called
     *      NtUserSetCursor(NULL). Reset to the default arrow instead of
     *      letting the previously applied custom pixmap stay on screen.
     *   3) id == 0 && bits != NULL -> custom ARGB cursor, keep existing
     *      SetCustomCursor path.
     */
    if (id > 0 || !bits)
    {
        TRACE( "hwnd %p system id %d (bits=%p)\n", hwnd, id, bits );
        SetCursorStyle(data->widget_id, id > 0 ? id - 1 : 0);
        return STATUS_SUCCESS;
    }

    if (width < 0 || height < 0 || width > 256 || height > 256)
        return STATUS_INVALID_PARAMETER;

    TRACE( "hwnd %p id %d\n", hwnd, id );

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
