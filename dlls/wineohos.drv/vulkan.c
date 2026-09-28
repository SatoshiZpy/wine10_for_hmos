/* OHOSDRV Vulkan implementation
 *
 * Copyright 2026 Wine OHOS Project
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <dlfcn.h>
#include <stdlib.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "ohos.h"
#include "wine/debug.h"

#include "wine/vulkan.h"
#include "wine/vulkan_driver.h"

WINE_DEFAULT_DEBUG_CHANNEL(vulkan);

#ifdef SONAME_LIBVULKAN

/*
 * OHOS Vulkan Extension Definitions
 * Reference: vulkan_ohos.h - VK_OHOS_surface extension
 * Reference: VkSurfaceCreateInfoOHOS.md
 */

/* VK_OHOS_surface extension constants */
#define VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS 1000006002

/* VkSurfaceCreateFlagsOHOS - reserved, must be 0 */
typedef VkFlags VkSurfaceCreateFlagsOHOS;

/* VkSurfaceCreateInfoOHOS structure
 * Reference: VkSurfaceCreateInfoOHOS.md
 */
typedef struct VkSurfaceCreateInfoOHOS
{
    VkStructureType sType;                  /* Must be VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS */
    const void *pNext;                      /* Must be NULL */
    VkSurfaceCreateFlagsOHOS flags;         /* Reserved, must be 0 */
    OHNativeWindow *window;                 /* OHNativeWindow pointer */
} VkSurfaceCreateInfoOHOS;

/* Function pointer type definition */
typedef VkResult (VKAPI_PTR *PFN_vkCreateSurfaceOHOS)(VkInstance, const VkSurfaceCreateInfoOHOS *, const VkAllocationCallbacks *, VkSurfaceKHR *);

/* OHOS Vulkan function pointers */
static PFN_vkCreateSurfaceOHOS pvkCreateSurfaceOHOS;

static const struct vulkan_driver_funcs ohos_vulkan_driver_funcs;

/* OHOS Vulkan Surface creation function
 * Reference: winewayland.drv/vulkan.c wayland_vulkan_surface_create
 */
static VkResult ohos_vulkan_surface_create(HWND hwnd, const struct vulkan_instance *instance,
                                           VkSurfaceKHR *handle, struct client_surface **client)
{
    VkResult res;
    VkSurfaceCreateInfoOHOS create_info_host;
    struct ohos_client_surface *surface;
    OHNativeWindow *native_window;

    TRACE("hwnd=%p instance=%p handle=%p client=%p\n", hwnd, instance, handle, client);

    /* Create OHOS client surface */
    if (!(surface = ohos_client_surface_create(hwnd, TRUE)))
        return VK_ERROR_OUT_OF_HOST_MEMORY;

    /* Get OHNativeWindow: the imported producer for a cross-process child,
     * the local XComponent window otherwise */
    native_window = surface->native_window ? surface->native_window
                                           : GetNativeWindow(surface->widget_id);
    if (!native_window)
    {
        ERR("Failed to get native window for widget_id %d\n", surface->widget_id);
        client_surface_release(&surface->client);
        return VK_ERROR_SURFACE_LOST_KHR;
    }

    /* Fill VkSurfaceCreateInfoOHOS structure
     * Reference: vulkan-guidelines.md development steps
     */
    create_info_host.sType = VK_STRUCTURE_TYPE_SURFACE_CREATE_INFO_OHOS;
    create_info_host.pNext = NULL;
    create_info_host.flags = 0;  /* Reserved, must be 0 */
    create_info_host.window = native_window;

    /* Call vkCreateSurfaceOHOS to create Vulkan Surface */
    res = pvkCreateSurfaceOHOS(instance->host.instance, &create_info_host,
                               NULL /* allocator */, handle);
    if (res != VK_SUCCESS)
    {
        ERR("Failed to create vulkan OHOS surface, res=%d\n", res);
        client_surface_release(&surface->client);
        return res;
    }

    *client = &surface->client;

    /* Mark the window as Vulkan-active so GDI surface flush is skipped.
     * When DXVK renders via Vulkan, the GDI path would overwrite
     * the Vulkan content with the window background color. */
    {
        struct ohos_win_data *data = get_win_data( hwnd );
        if (data)
        {
            data->vulkan_active = TRUE;
            TRACE("hwnd %p marked as vulkan_active\n", hwnd);
            release_win_data( data );
        }
    }

    TRACE("Created surface=0x%s, client=%p\n", wine_dbgstr_longlong(*handle), *client);
    return VK_SUCCESS;
}

/* Check if physical device supports OHOS presentation
 * Note: OHOS doesn't have direct equivalent function, check via vkGetPhysicalDeviceSurfaceSupportKHR
 */
static VkBool32 ohos_vkGetPhysicalDeviceWin32PresentationSupportKHR(VkPhysicalDevice phys_dev,
                                                                    uint32_t index)
{
    TRACE("phys_dev=%p index=%u\n", phys_dev, index);

    /* On OHOS, we can't directly check Win32 presentation support
     * Return TRUE, let winevulkan verify later via vkGetPhysicalDeviceSurfaceSupportKHR
     */
    return TRUE;
}

/* Return OHOS platform Vulkan Surface extension name */
static const char *ohos_get_host_surface_extension(void)
{
    return "VK_OHOS_surface";
}

/* OHOS Vulkan driver function table */
static const struct vulkan_driver_funcs ohos_vulkan_driver_funcs =
{
    .p_vulkan_surface_create = ohos_vulkan_surface_create,
    .p_vkGetPhysicalDeviceWin32PresentationSupportKHR = ohos_vkGetPhysicalDeviceWin32PresentationSupportKHR,
    .p_get_host_surface_extension = ohos_get_host_surface_extension,
};

/**********************************************************************
 *           OHOS_VulkanInit
 *
 * Wine Vulkan driver initialization entry point
 * Called by vulkan_driver_init() in win32u.dll
 */
UINT OHOS_VulkanInit(UINT version, void *vulkan_handle, const struct vulkan_driver_funcs **driver_funcs)
{
    if (version != WINE_VULKAN_DRIVER_VERSION)
    {
        ERR("version mismatch, win32u wants %u but driver has %u\n",
            version, WINE_VULKAN_DRIVER_VERSION);
        return STATUS_INVALID_PARAMETER;
    }

    /* Load OHOS Vulkan extension functions from libvulkan.so
     * Reference: vulkan-guidelines.md - need to define VK_USE_PLATFORM_OHOS=1
     */
#define LOAD_FUNCPTR(f) if (!(p##f = dlsym(vulkan_handle, #f))) return STATUS_PROCEDURE_NOT_FOUND;
    LOAD_FUNCPTR(vkCreateSurfaceOHOS);
#undef LOAD_FUNCPTR

    *driver_funcs = &ohos_vulkan_driver_funcs;
    return STATUS_SUCCESS;
}

#else /* No vulkan */

UINT OHOS_VulkanInit(UINT version, void *vulkan_handle, const struct vulkan_driver_funcs **driver_funcs)
{
    ERR("Wine was built without Vulkan support.\n");
    return STATUS_NOT_IMPLEMENTED;
}

#endif /* SONAME_LIBVULKAN */
