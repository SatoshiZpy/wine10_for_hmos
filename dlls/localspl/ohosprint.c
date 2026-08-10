#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdlib.h>

#include "windef.h"
#include "wine/debug.h"

#include "localspl_private.h"

#ifdef __OHOS__
#include "../../ohos/adapter/include/ohos_print.h"
#endif

WINE_DEFAULT_DEBUG_CHANNEL(localspl);

#define OHOS_PRINT_MAX_SIZE (64 * 1024 * 1024)
#define OHOS_PRINT_HEADER_SCAN_SIZE (64 * 1024)

struct ohos_print_doc
{
    char *printer_id;
    char *document_title;
    BYTE *data;
    size_t size;
    size_t capacity;
};

static char *strdup_utf8(const WCHAR *str)
{
    unsigned int len;
    char *ret;

    if (!str) return NULL;
    len = wcslen(str);
    if (!(ret = malloc(len * 3 + 1))) return NULL;
    ntdll_wcstoumbs(str, len + 1, ret, len * 3 + 1, FALSE);
    return ret;
}

struct ohos_print_doc *ohos_print_start_doc(const WCHAR *printer_id,
        const WCHAR *document_title)
{
    struct ohos_print_doc *doc;

    TRACE("start in-memory PostScript spool\n");
    if (!(doc = calloc(1, sizeof(*doc)))) return NULL;
    if (!(doc->printer_id = strdup_utf8(printer_id))) goto failed;
    if (document_title && !(doc->document_title = strdup_utf8(document_title))) goto failed;
    return doc;

failed:
    free(doc->printer_id);
    free(doc->document_title);
    free(doc);
    return NULL;
}

BOOL ohos_print_write_doc(struct ohos_print_doc *doc, const BYTE *buf, unsigned int size)
{
    size_t needed = doc->size + size;
    BYTE *data;

    TRACE("write PostScript chunk=%u total=%zu\n", size, doc->size);
    if (size > OHOS_PRINT_MAX_SIZE - doc->size)
    {
        ERR("PostScript print job exceeds %u bytes\n", OHOS_PRINT_MAX_SIZE);
        return FALSE;
    }
    if (needed > doc->capacity)
    {
        size_t capacity = max(needed, min(doc->capacity ? doc->capacity * 2 : 65536,
                (size_t)OHOS_PRINT_MAX_SIZE));
        if (!(data = realloc(doc->data, capacity))) return FALSE;
        doc->data = data;
        doc->capacity = capacity;
    }
    memcpy(doc->data + doc->size, buf, size);
    doc->size = needed;
    return TRUE;
}

BOOL ohos_print_end_doc(struct ohos_print_doc *doc)
{
    BOOL ret = FALSE;
    size_t offset, scan_size = min(doc->size, (size_t)OHOS_PRINT_HEADER_SCAN_SIZE);

    TRACE("finish PostScript size=%zu\n", doc->size);
    for (offset = 0; offset + 4 <= scan_size; offset++)
        if (!memcmp(doc->data + offset, "%!PS", 4)) break;

    if (offset + 4 > scan_size)
        TRACE("refusing data without a PostScript header\n");
#ifdef __OHOS__
    else
    {
        TRACE("PostScript header offset=%zu, payload size=%zu\n", offset, doc->size - offset);
        ret = !OhosPrintPostScript(doc->printer_id, doc->document_title,
                doc->data + offset, doc->size - offset);
    }
#else
    ERR("OHOS PostScript printing is unavailable on this platform\n");
#endif
    free(doc->data);
    free(doc->printer_id);
    free(doc->document_title);
    free(doc);
    return ret;
}
