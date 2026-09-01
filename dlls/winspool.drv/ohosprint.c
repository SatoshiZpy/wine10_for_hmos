#ifdef __OHOS__ // __OHOS__

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS

#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winuser.h"
#include "winerror.h"
#include "winreg.h"
#include "wingdi.h"
#include "winspool.h"
#include "ddk/winsplp.h"

#include "wine/debug.h"

#include "ohos_adapter.h"
#include "wspool.h"

WINE_DEFAULT_DEBUG_CHANNEL(winspool);

static WCHAR *copy_utf8_string( WCHAR **buffer, const char *source )
{
    WCHAR *ret = *buffer;
    DWORD size = 0;

    RtlUTF8ToUnicodeN( ret, (strlen( source ) + 1) * sizeof(WCHAR), &size,
                       source, strlen( source ) + 1 );
    *buffer += size / sizeof(WCHAR);
    return ret;
}

NTSTATUS ohos_enum_printers( const struct enum_printers_params *params )
{
    struct ohos_printer_info *host_printers;
    unsigned int i, num, needed;
    WCHAR *ptr;
    int count;

    *params->num = 0;
    count = GetPrinters( NULL, 0 );
    if (count < 0) return STATUS_NOT_SUPPORTED;
    if (!count) return STATUS_SUCCESS;

    if (!(host_printers = calloc( count, sizeof(*host_printers) )))
        return STATUS_NO_MEMORY;

    count = GetPrinters( host_printers, count );
    if (count < 0)
    {
        free( host_printers );
        return STATUS_NOT_SUPPORTED;
    }

    num = count;
    needed = sizeof(struct printer_info) * num
           + sizeof(struct ohos_printer_details) * num;

    for (i = 0; i < num; ++i)
    {
        needed += (strlen(host_printers[i].name)
                + strlen(host_printers[i].description)
                + strlen(host_printers[i].uri)
                + strlen(host_printers[i].id)
                + strlen(host_printers[i].make)
                + strlen(host_printers[i].page_sizes)
                + strlen(host_printers[i].options) + 7) * sizeof(WCHAR);
    }

    *params->num = num;
    if (needed > *params->size)
    {
        *params->size = needed;
        free( host_printers );
        return STATUS_BUFFER_OVERFLOW;
    }

    ptr = (WCHAR *)((struct ohos_printer_details *)(params->printers + num) + num);

    for (i = 0; i < num; ++i)
    {
        struct printer_info *info = params->printers + i;

        info->ohos_info =
            (struct ohos_printer_details *)(params->printers + num) + i;

#define COPY_HOST_STRING(member, source) \
        info->member = copy_utf8_string( &ptr, source )

        COPY_HOST_STRING(name, host_printers[i].name);
        COPY_HOST_STRING(comment, host_printers[i].description);
        COPY_HOST_STRING(location, host_printers[i].uri);
        COPY_HOST_STRING(ohos_info->id, host_printers[i].id);
        COPY_HOST_STRING(ohos_info->make, host_printers[i].make);
        COPY_HOST_STRING(ohos_info->uri, host_printers[i].uri);
        COPY_HOST_STRING(ohos_info->page_sizes, host_printers[i].page_sizes);
        COPY_HOST_STRING(ohos_info->options, host_printers[i].options);

#undef COPY_HOST_STRING

        info->ohos_info->status = host_printers[i].status;
        info->ohos_info->color_modes = host_printers[i].color_modes;
        info->ohos_info->duplex_modes = host_printers[i].duplex_modes;
        info->is_default = host_printers[i].is_default;
    }

    free( host_printers );
    return STATUS_SUCCESS;
}

#endif // __OHOS__
