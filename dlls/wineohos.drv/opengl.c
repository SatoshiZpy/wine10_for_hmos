#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <assert.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "ohos.h"
#include "winternl.h"

#include "wine/opengl_driver.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ohos);

static const struct egl_platform *egl;
static const struct opengl_funcs *funcs;
static const struct opengl_drawable_funcs ohos_drawable_funcs;

struct gl_drawable
{
    struct opengl_drawable base;
    int widget_id;
};

static struct gl_drawable *impl_from_opengl_drawable( struct opengl_drawable *base )
{
    return CONTAINING_RECORD( base, struct gl_drawable, base );
}

static void *opengl_handle;

static inline EGLConfig egl_config_for_format(int format)
{
    assert(format > 0 && format <= 2 * egl->config_count);
    if (format <= egl->config_count) return egl->configs[format - 1];
    return egl->configs[format - egl->config_count - 1];
}

static struct gl_drawable *create_gl_drawable( HWND hwnd, HDC hdc, int format )
{
    static const int attribs[] = { EGL_WIDTH, 1, EGL_HEIGHT, 1, EGL_NONE };
    struct ohos_client_surface *client;
    struct gl_drawable *gl;

    if (!(client = ohos_client_surface_create( hwnd, TRUE ))) return NULL;
    gl = opengl_drawable_create( sizeof(*gl), &ohos_drawable_funcs, format, &client->client );
    client_surface_release( &client->client );
    if (!gl) return NULL;


    gl->widget_id = client->widget_id;

    if (client->native_window)
        gl->base.surface = funcs->p_eglCreateWindowSurface( egl->display, egl_config_for_format(gl->base.format), client->native_window, NULL );
    else if (gl->widget_id <= 0) gl->base.surface = funcs->p_eglCreatePbufferSurface( egl->display, egl_config_for_format(gl->base.format), attribs );
    else gl->base.surface = funcs->p_eglCreateWindowSurface( egl->display, egl_config_for_format(gl->base.format), GetNativeWindow(gl->widget_id), NULL );

    TRACE( "Created drawable %s with client window %d\n", debugstr_opengl_drawable( &gl->base ), gl->widget_id );
    return gl;
}

static void ohos_drawable_destroy( struct opengl_drawable *base )
{
    struct gl_drawable *gl = impl_from_opengl_drawable( base );

    TRACE( "drawable %s surface %p\n", debugstr_opengl_drawable( base ), gl->base.surface );
}

static BOOL ohos_surface_create( HWND hwnd, HDC hdc, int format, struct opengl_drawable **drawable )
{
    struct gl_drawable *gl;

    TRACE( "hwnd %p, hdc %p, format %d, drawable %p\n", hwnd, hdc, format, drawable );

    if (*drawable)
    {
        EGLint pf;

        FIXME( "Updating drawable %s, multiple surfaces not implemented\n", debugstr_opengl_drawable( *drawable ) );

        gl = impl_from_opengl_drawable( *drawable );
        funcs->p_eglGetConfigAttrib( egl->display, egl_config_for_format(format), EGL_NATIVE_VISUAL_ID, &pf );
        {
            struct ohos_client_surface *ohos_client =
                CONTAINING_RECORD( gl->base.client, struct ohos_client_surface, client );
            ohos_native_window native_window =
                ohos_client->native_window ? ohos_client->native_window
                                           : GetNativeWindow(gl->widget_id);
            int ret = OH_NativeWindow_NativeWindowHandleOpt(native_window, SET_FORMAT, pf);
            if (ret != 0) {
                ERR("set format error: %d", ret);
            }
        }
        gl->base.format = format;

        TRACE( "Updated drawable %s\n", debugstr_opengl_drawable( *drawable ) );
        return TRUE;
    }

    if (!(gl = create_gl_drawable( hwnd, hdc, format ))) return FALSE;
    *drawable = &gl->base;
    return TRUE;
}

static void ohos_init_egl_platform( struct egl_platform *platform )
{
    /* Mesa's OHOS EGL backend provides desktop OpenGL through ZINK. */
    platform->type = EGL_PLATFORM_OHOS_KHR;
    platform->native_display = EGL_DEFAULT_DISPLAY;
    egl = platform;
}

static UINT ohos_init_pixel_formats( UINT *onscreen_count )
{
    static const EGLint gl_attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE };
    static const EGLint es3_attribs[] = { EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT, EGL_NONE };
    struct egl_platform *platform = (struct egl_platform *)egl;
    const EGLint *attribs = gl_attribs;
    EGLConfig *configs = NULL;
    EGLint count = 0;

    /* The OHOS system EGL exposes GLES configs only; desktop GL is provided
     * by the libGLv4 wrapper on top of them. */
    funcs->p_eglChooseConfig( platform->display, attribs, NULL, 0, &count );
    if (count <= 0)
    {
        TRACE( "no EGL_OPENGL_BIT configs, falling back to ES3 configs\n" );
        attribs = es3_attribs;
        funcs->p_eglChooseConfig( platform->display, attribs, NULL, 0, &count );
    }
    if (count <= 0)
    {
        ERR( "Failed to get any configs from eglChooseConfig\n" );
        return 0;
    }
    if (!(configs = malloc( count * sizeof(*configs) ))) return 0;
    if (!funcs->p_eglChooseConfig( platform->display, attribs, configs, count, &count ) || !count)
    {
        ERR( "Failed to get configs from eglChooseConfig\n" );
        free( configs );
        return 0;
    }

    platform->configs = configs;
    platform->config_count = count;
    *onscreen_count = count;
    TRACE( "obtained %u EGL configs\n", count );
    return 2 * count;
}

static void *ohos_get_proc_address( const char *name )
{
    void *ptr;
    if ((ptr = dlsym( opengl_handle, name ))) return ptr;
    return funcs->p_eglGetProcAddress( name );
}

static BOOL ohos_drawable_swap( struct opengl_drawable *base )
{
    struct gl_drawable *gl = impl_from_opengl_drawable( base );

    TRACE( "drawable %s surface %p\n", debugstr_opengl_drawable( base ), gl->base.surface );

    funcs->p_eglSwapBuffers( egl->display, gl->base.surface );
    return TRUE;
}

static void ohos_drawable_flush( struct opengl_drawable *base, UINT flags )
{
    struct gl_drawable *gl = impl_from_opengl_drawable( base );

    TRACE( "drawable %s, surface %p, flags %#x\n", debugstr_opengl_drawable( base ), gl->base.surface, flags );

    if (flags & GL_FLUSH_INTERVAL) funcs->p_eglSwapInterval( egl->display, abs( base->interval ) );
}

static struct opengl_driver_funcs ohos_driver_funcs =
{
    .p_init_egl_platform = ohos_init_egl_platform,
    .p_get_proc_address = ohos_get_proc_address,
    .p_surface_create = ohos_surface_create,
};

static const struct opengl_drawable_funcs ohos_drawable_funcs =
{
    .destroy = ohos_drawable_destroy,
    .flush = ohos_drawable_flush,
    .swap = ohos_drawable_swap,
};

/**********************************************************************
 *           OHOS__OpenGLInit
 */
UINT OHOS_OpenGLInit( UINT version, const struct opengl_funcs *opengl_funcs, const struct opengl_driver_funcs **driver_funcs )
{
    if (version != WINE_OPENGL_DRIVER_VERSION)
    {
        ERR( "version mismatch, opengl32 wants %u but driver has %u\n", version, WINE_OPENGL_DRIVER_VERSION );
        return STATUS_INVALID_PARAMETER;
    }
    if (!opengl_funcs->egl_handle) return STATUS_NOT_SUPPORTED;
    if (!(opengl_handle = dlopen( SONAME_LIBGLESV3, RTLD_NOW|RTLD_GLOBAL )))
    {
        ERR( "failed to load %s: %s\n", SONAME_LIBGLESV3, dlerror() );
        return STATUS_NOT_SUPPORTED;
    }
    funcs = opengl_funcs;

    ohos_driver_funcs.p_init_pixel_formats = ohos_init_pixel_formats;
    ohos_driver_funcs.p_describe_pixel_format = (*driver_funcs)->p_describe_pixel_format;
    ohos_driver_funcs.p_init_wgl_extensions = (*driver_funcs)->p_init_wgl_extensions;
    ohos_driver_funcs.p_context_create = (*driver_funcs)->p_context_create;
    ohos_driver_funcs.p_context_destroy = (*driver_funcs)->p_context_destroy;
    ohos_driver_funcs.p_make_current = (*driver_funcs)->p_make_current;

    *driver_funcs = &ohos_driver_funcs;
    return STATUS_SUCCESS;
}
