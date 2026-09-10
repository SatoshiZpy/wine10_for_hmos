/*
 * OHOS clipboard driver
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include <database/pasteboard/oh_pasteboard.h>
#include <database/udmf/udmf.h>
#include <database/udmf/uds.h>
#include <multimedia/image_framework/image/pixelmap_native.h>
#include "windef.h"
#include "winuser.h"
#include "shlobj.h"
#include "ntuser.h"
#include "winternl.h"
#include "wine/unixlib.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(ohos);

#ifndef PASTEBOARD_MIMETYPE_TEXT_PLAIN
#define PASTEBOARD_MIMETYPE_TEXT_PLAIN "text/plain"
#endif
#ifndef PASTEBOARD_MIMETYPE_TEXT_HTML
#define PASTEBOARD_MIMETYPE_TEXT_HTML "text/html"
#endif
#ifndef PASTEBOARD_MIMETYPE_TEXT_URI
#define PASTEBOARD_MIMETYPE_TEXT_URI "text/uri"
#endif
#ifndef PASTEBOARD_MIMETYPE_PIXELMAP
#define PASTEBOARD_MIMETYPE_PIXELMAP "pixelMap"
#endif

/* runtime permission request into the ohos adapter (libadapter);
 * implemented in ohos/adapter/permission_manager/permission_manager_adapter.cc */
extern int OhosAdapterRequestPermission(int permission_type);
enum { OHOS_PERMISSION_PASTEBOARD = 6 };  /* OHOSPermissionType::PASTEBOARD */

/**************************************************************************
 *              Constants
 **************************************************************************/

#define PBS_POLL_INTERVAL_MS 100
#define CLIPBOARD_SYNC_MIN_INTERVAL_MS 50

/**************************************************************************
 *              Data carried between the pbs worker and wine threads
 **************************************************************************/

/* raw RGBA_8888 image, row stride = width * 4, caller frees bits */
struct clip_image
{
    UINT width;
    UINT height;
    BYTE *bits;
};

/* pending clipboard payload, one field per format kind; NULL = absent */
struct pending_clip
{
    char *text;             /* UTF-8, NUL-terminated */
    char *html;             /* UTF-8 html fragment, NUL-terminated */
    char *urilist;          /* "file://...\r\n" lines, NUL-terminated */
    struct clip_image *image;
};

static pthread_mutex_t state_mutex = PTHREAD_MUTEX_INITIALIZER;

/* OHOS -> win32: written by the pbs worker, stolen by a wine thread */
static struct pending_clip import_pending;

/* win32 -> OHOS: written by a wine thread, taken by the pbs worker */
static struct pending_clip export_pending;

static pthread_t pbs_thread;
static volatile LONG pbs_thread_started;

static DWORD last_win32_seq; /* win32 clipboard sequence number (wine side) */

/* READ_PASTEBOARD runtime permission state (accessed with Interlocked* only) */
static volatile LONG import_denied;        /* pasteboard read was rejected by PBS */
static volatile LONG perm_request_pending; /* wine side asks the worker to request */
static volatile LONG perm_inflight;        /* worker is inside the request (dialog up) */
static volatile LONG perm_denied_by_user;  /* user explicitly refused: stop asking */


/**************************************************************************
 *              Shared-buffer helpers
 **************************************************************************/

static void free_clip_image( struct clip_image *image )
{
    if (!image) return;
    free( image->bits );
    free( image );
}

static void free_pending_clip( struct pending_clip *clip )
{
    free( clip->text );
    free( clip->html );
    free( clip->urilist );
    free_clip_image( clip->image );
    memset( clip, 0, sizeof(*clip) );
}

/* steal the pending import payload (caller frees), all-NULL if none */
static struct pending_clip take_import_pending(void)
{
    struct pending_clip ret;

    pthread_mutex_lock(&state_mutex);
    ret = import_pending;
    memset(&import_pending, 0, sizeof(import_pending));
    pthread_mutex_unlock(&state_mutex);
    return ret;
}

/* publish a freshly extracted import payload, replacing (and freeing) any stale one */
static void put_import_pending( struct pending_clip *clip )
{
    pthread_mutex_lock(&state_mutex);
    free_pending_clip(&import_pending);
    import_pending = *clip;
    pthread_mutex_unlock(&state_mutex);
}

/* steal the pending export payload (caller frees), all-NULL if none */
static struct pending_clip take_export_pending(void)
{
    struct pending_clip ret;

    pthread_mutex_lock(&state_mutex);
    ret = export_pending;
    memset(&export_pending, 0, sizeof(export_pending));
    pthread_mutex_unlock(&state_mutex);
    return ret;
}

/* publish win32 data for the worker to push, replacing any stale one */
static void put_export_pending( struct pending_clip *clip )
{
    pthread_mutex_lock(&state_mutex);
    free_pending_clip(&export_pending);
    export_pending = *clip;
    pthread_mutex_unlock(&state_mutex);
}


 /**************************************************************************
  *              OHOS UDMF helpers (called on the pbs worker only)
  **************************************************************************/

static char *dup_cstr( const char *s )
{
    size_t len;
    char *ret;

    if (!s || !*s) return NULL;
    len = strlen(s);
    if (!(ret = malloc(len + 1))) return NULL;
    memcpy(ret, s, len + 1);
    return ret;
}

/**************************************************************************
 *              worker_extract_import
 *
 * Read every supported format out of the current pasteboard data.
 * Fills *out (ownership transferred to caller); returns TRUE if anything
 * was found.
 */
static BOOL worker_extract_import( OH_Pasteboard *pbs, struct pending_clip *out )
{
    OH_UdmfData *ud;
    int ret = 0;
    BOOL any = FALSE;

    memset(out, 0, sizeof(*out));

    if (!(ud = OH_Pasteboard_GetData(pbs, &ret)))
    {
        WARN("DIAGCLIP import: GetData failed ret=%d\n", ret);
        /* most likely READ_PASTEBOARD is not granted: remember it; the
         * runtime request is deferred until a win32 app reads the clipboard */
        InterlockedExchange(&import_denied, 1);
        return FALSE;
    }
    InterlockedExchange(&import_denied, 0);

    /* plain text */
    if (OH_Pasteboard_HasType(pbs, PASTEBOARD_MIMETYPE_TEXT_PLAIN))
    {
        OH_UdsPlainText *plain_text = OH_UdsPlainText_Create();
        if (plain_text)
        {
            if (OH_UdmfData_GetPrimaryPlainText(ud, plain_text) == 0)
            {
                const char *content = OH_UdsPlainText_GetContent(plain_text);
                if (content && *content)
                {
                    out->text = dup_cstr(content);
                    any = TRUE;
                }
            }
            OH_UdsPlainText_Destroy(plain_text);
        }
    }

    /* html */
    if (OH_Pasteboard_HasType(pbs, PASTEBOARD_MIMETYPE_TEXT_HTML))
    {
        OH_UdsHtml *html = OH_UdsHtml_Create();
        if (html)
        {
            if (OH_UdmfData_GetPrimaryHtml(ud, html) == 0)
            {
                const char *content = OH_UdsHtml_GetContent(html);
                if (content && *content)
                {
                    out->html = dup_cstr(content);
                    any = TRUE;
                }
            }
            OH_UdsHtml_Destroy(html);
        }
    }

    /* file uris: walk the records.  GetRecord returns a borrowed pointer --
     * never destroy it, the OH_UdmfData_Destroy below owns them all. */
    if (OH_Pasteboard_HasType(pbs, PASTEBOARD_MIMETYPE_TEXT_URI))
    {
        int count = OH_UdmfData_GetRecordCount(ud);
        char *list = NULL;
        size_t len = 0;
        int i;

        for (i = 0; i < count; i++)
        {
            OH_UdmfRecord *record = OH_UdmfData_GetRecord(ud, i);
            OH_UdsFileUri *file_uri;
            const char *uri;
            size_t uri_len;
            char *new_list;

            if (!record) continue;
            if (!(file_uri = OH_UdsFileUri_Create())) continue;
            if (OH_UdmfRecord_GetFileUri(record, file_uri) != 0 ||
                !(uri = OH_UdsFileUri_GetFileUri(file_uri)) ||
                strncmp(uri, "file:", 5) != 0)
            {
                OH_UdsFileUri_Destroy(file_uri);
                continue;
            }
            uri_len = strlen(uri);
            if ((new_list = realloc(list, len + uri_len + 3)))
            {
                list = new_list;
                memcpy(list + len, uri, uri_len);
                len += uri_len;
                list[len++] = '\r';
                list[len++] = '\n';
            }
            OH_UdsFileUri_Destroy(file_uri);
        }

        if (list)
        {
            list[len] = 0;
            out->urilist = list;
            any = TRUE;
        }
    }

    /* pixelmap import: FIXME -- the NDK API OH_UdsPixelMap_GetPixelMap()
     * cannot hand out the OH_PixelmapNative* it holds (its "output"
     * parameter is a plain pointer), so OHOS -> win32 image paste is not
     * implementable with this SDK.  win32 -> OHOS image export is
     * supported below. */

    OH_UdmfData_Destroy(ud);

    if (!any) WARN("DIAGCLIP import: pasteboard had no supported data\n");
    return any;
}


/**************************************************************************
 *              worker_push_export
 *
 * Compose one multi-record OH_UdmfData out of the pending win32 payload
 * and push it to the pasteboard.  Runs on the pbs worker only.
 */
static void worker_push_export( OH_Pasteboard *pbs, struct pending_clip *exp )
{
    OH_UdmfData *data;
    BOOL have_record = FALSE;

    if (!exp->text && !exp->html && !exp->urilist && !exp->image) return;

    if (!(data = OH_UdmfData_Create())) return;

    /* plain text record */
    if (exp->text)
    {
        OH_UdsPlainText *plain_text = OH_UdsPlainText_Create();
        OH_UdmfRecord *record = NULL;

        if (plain_text && OH_UdsPlainText_SetContent(plain_text, exp->text) == 0)
            record = OH_UdmfRecord_Create();

        if (record)
        {
            if (OH_UdmfRecord_AddPlainText(record, plain_text) == 0)
            {
                /* plain_text now owned by record */
                if (OH_UdmfData_AddRecord(data, record) == 0)
                    have_record = TRUE;             /* record owned by data */
                else
                    OH_UdmfRecord_Destroy(record);  /* frees plain_text too */
            }
            else
            {
                OH_UdmfRecord_Destroy(record);      /* plain_text not added */
                OH_UdsPlainText_Destroy(plain_text);
            }
        }
        else if (plain_text)
            OH_UdsPlainText_Destroy(plain_text);

        free(exp->text);
        exp->text = NULL;
    }

    /* html record */
    if (exp->html)
    {
        OH_UdsHtml *html = OH_UdsHtml_Create();
        OH_UdmfRecord *record = NULL;

        if (html && OH_UdsHtml_SetContent(html, exp->html) == 0)
            record = OH_UdmfRecord_Create();

        if (record)
        {
            if (OH_UdmfRecord_AddHtml(record, html) == 0)
            {
                if (OH_UdmfData_AddRecord(data, record) == 0)
                    have_record = TRUE;
                else
                    OH_UdmfRecord_Destroy(record);
            }
            else
            {
                OH_UdmfRecord_Destroy(record);
                OH_UdsHtml_Destroy(html);
            }
        }
        else if (html)
            OH_UdsHtml_Destroy(html);

        free(exp->html);
        exp->html = NULL;
    }

    /* file uri records: one record per uri line */
    if (exp->urilist)
    {
        char *line = exp->urilist;

        while (*line)
        {
            char *end = strstr(line, "\r\n");
            size_t len = end ? (size_t)(end - line) : strlen(line);
            OH_UdsFileUri *file_uri = OH_UdsFileUri_Create();
            OH_UdmfRecord *record = NULL;

            if (file_uri && len < 512)
            {
                char uri[512];
                memcpy(uri, line, len);
                uri[len] = 0;
                if (OH_UdsFileUri_SetFileUri(file_uri, uri) == 0)
                    record = OH_UdmfRecord_Create();
            }

            if (record)
            {
                if (OH_UdmfRecord_AddFileUri(record, file_uri) == 0)
                {
                    if (OH_UdmfData_AddRecord(data, record) == 0)
                        have_record = TRUE;
                    else
                        OH_UdmfRecord_Destroy(record);
                }
                else
                {
                    OH_UdmfRecord_Destroy(record);
                    OH_UdsFileUri_Destroy(file_uri);
                }
            }
            else if (file_uri)
                OH_UdsFileUri_Destroy(file_uri);

            if (!end) break;
            line = end + 2;
        }

        free(exp->urilist);
        exp->urilist = NULL;
    }

    /* pixelmap record (win32 CF_DIB -> RGBA_8888 pixelmap) */
    if (exp->image)
    {
        OH_Pixelmap_InitializationOptions *opts = NULL;
        OH_PixelmapNative *native = NULL;

        if (OH_PixelmapInitializationOptions_Create(&opts) == IMAGE_SUCCESS)
        {
            OH_PixelmapInitializationOptions_SetWidth(opts, exp->image->width);
            OH_PixelmapInitializationOptions_SetHeight(opts, exp->image->height);
            OH_PixelmapInitializationOptions_SetPixelFormat(opts, PIXEL_FORMAT_RGBA_8888);
            OH_PixelmapInitializationOptions_SetAlphaType(opts, PIXELMAP_ALPHA_TYPE_PREMULTIPLIED);
            OH_PixelmapInitializationOptions_SetRowStride(opts, exp->image->width * 4);

            if (OH_PixelmapNative_CreatePixelmap(exp->image->bits,
                                                 (size_t)exp->image->width * exp->image->height * 4,
                                                 opts, &native) == IMAGE_SUCCESS && native)
            {
                OH_UdsPixelMap *pixel_map = OH_UdsPixelMap_Create();
                OH_UdmfRecord *record = NULL;

                if (pixel_map && OH_UdsPixelMap_SetPixelMap(pixel_map, native) == 0)
                    record = OH_UdmfRecord_Create();

                if (record)
                {
                    if (OH_UdmfRecord_AddPixelMap(record, pixel_map) == 0)
                    {
                        if (OH_UdmfData_AddRecord(data, record) == 0)
                            have_record = TRUE;
                        else
                            OH_UdmfRecord_Destroy(record);
                    }
                    else
                    {
                        OH_UdmfRecord_Destroy(record);
                        OH_UdsPixelMap_Destroy(pixel_map);
                    }
                }
                else if (pixel_map)
                    OH_UdsPixelMap_Destroy(pixel_map);
            }
            OH_PixelmapInitializationOptions_Release(opts);
        }

        free_clip_image(exp->image);
        exp->image = NULL;
    }

    if (have_record)
    {
        int set_ret = OH_Pasteboard_SetData(pbs, data);
        WARN("DIAGCLIP export: pushed multi-record data ret=%d change=%u\n",
             set_ret, OH_Pasteboard_GetChangeCount(pbs));
    }

    /* single owner: frees all records it holds */
    OH_UdmfData_Destroy(data);
}


/**************************************************************************
 *              pbs_worker
 *
 * Raw pthread owning the OHOS pasteboard object.  Binder-only world:
 * never touches NtUser or the wineserver.  Polls the pasteboard,
 * prefetches new content, and services export requests.
 */
static void *pbs_worker(void *arg)
{
    OH_Pasteboard *pbs;
    uint32_t seen = 0;
    struct pending_clip exp;

    pbs = OH_Pasteboard_Create();
    if (!pbs)
    {
        ERR("DIAGCLIP pbs_worker: OH_Pasteboard_Create failed\n");
        return NULL;
    }
    WARN("DIAGCLIP pbs_worker started\n");
    seen = OH_Pasteboard_GetChangeCount(pbs);

    for (;;)
    {
        /* 1. push pending win32 data to the OHOS pasteboard */
        exp = take_export_pending();
        if (exp.text || exp.html || exp.urilist || exp.image)
        {
            worker_push_export(pbs, &exp);
            /* don't import our own change back */
            seen = OH_Pasteboard_GetChangeCount(pbs);
        }

        /* 2. check the OHOS pasteboard for new content, prefetch it */
        {
            uint32_t cc = OH_Pasteboard_GetChangeCount(pbs);
            if (cc != seen)
            {
                struct pending_clip imp;

                seen = cc;
                if (OH_Pasteboard_HasData(pbs) &&
                    (OH_Pasteboard_HasType(pbs, PASTEBOARD_MIMETYPE_TEXT_PLAIN) ||
                     OH_Pasteboard_HasType(pbs, PASTEBOARD_MIMETYPE_TEXT_HTML) ||
                     OH_Pasteboard_HasType(pbs, PASTEBOARD_MIMETYPE_TEXT_URI)))
                {
                    if (worker_extract_import(pbs, &imp))
                    {
                        WARN("DIAGCLIP import: prefetched text=%d html=%d uri=%d change=%u\n",
                             imp.text ? 1 : 0, imp.html ? 1 : 0, imp.urilist ? 1 : 0, cc);
                        put_import_pending(&imp);
                    }
                }
                else
                    WARN("DIAGCLIP import: no supported type, skip change=%u\n", cc);
            }
        }

        /* 3. runtime READ_PASTEBOARD request, deferred from the wine side:
         * runs here on the worker so no wine thread blocks on the dialog */
        if (InterlockedCompareExchange(&perm_request_pending, 0, 0))
        {
            struct pending_clip imp;
            int ret;

            InterlockedExchange(&perm_request_pending, 0);
            InterlockedExchange(&perm_inflight, 1);

            ret = OhosAdapterRequestPermission(OHOS_PERMISSION_PASTEBOARD);
            WARN("DIAGCLIP permission: request READ_PASTEBOARD ret=%d\n", ret);

            if (ret == 0)
            {
                /* granted: refetch the current pasteboard content now, so the
                 * next clipboard query by a win32 app sees the data */
                if (worker_extract_import(pbs, &imp))
                    put_import_pending(&imp);
                else
                    WARN("DIAGCLIP permission: nothing to import after grant\n");
            }
            else if (ret == -1)
                InterlockedExchange(&perm_denied_by_user, 1);

            InterlockedExchange(&perm_inflight, 0);
        }

        usleep(PBS_POLL_INTERVAL_MS * 1000);
    }
    return NULL;
}


 /**************************************************************************
  *              Win32 clipboard side (wine threads only)
  **************************************************************************/

/* atom-based id for the registered "HTML Format" clipboard format */
static UINT html_format_id(void)
{
    static UINT id;
    if (!id)
    {
        static const WCHAR nameW[] = {'H','T','M','L',' ','F','o','r','m','a','t',0};
        RTL_ATOM atom;
        if (!NtAddAtom(nameW, (wcslen(nameW)) * sizeof(WCHAR), &atom)) id = atom;
    }
    return id;
}

/* read raw clipboard data (clipboard must be open); caller frees */
static void *read_clipboard_data( UINT format, size_t *ret_size )
{
    struct get_clipboard_params params = { .data_only = TRUE, .size = 1024 };
    void *data;

    for (;;)
    {
        if (!(data = malloc(params.size))) return NULL;
        params.data = data;
        if (NtUserGetClipboardData(format, &params))
        {
            *ret_size = params.size;
            return data;
        }
        free(data);
        if (!params.data_size) return NULL;
        params.size = params.data_size;
        params.data_size = 0;
    }
}

/* UTF-8 (LF line endings) -> UTF-16 with CRLF, NUL-terminated;
 * returns malloc'd buffer, *ret_size = byte size incl. final NUL */
static WCHAR *utf8_to_crlf_uni( const char *utf8, DWORD *ret_size )
{
    size_t src_len = strlen(utf8), i, j, count = 0;
    char *crlf;
    WCHAR *text;
    DWORD bytes;

    for (i = 0; i < src_len; i++) if (utf8[i] == '\n') count++;
    if (!(crlf = malloc(src_len + count + 1))) return NULL;
    for (i = j = 0; i < src_len; i++)
    {
        if (utf8[i] == '\n') crlf[j++] = '\r';
        crlf[j++] = utf8[i];
    }
    crlf[j] = 0;

    text = malloc((j + 1) * sizeof(WCHAR));
    if (!text) { free(crlf); return NULL; }
    RtlUTF8ToUnicodeN(text, j * sizeof(WCHAR), &bytes, crlf, j);
    text[bytes / sizeof(WCHAR)] = 0;
    free(crlf);
    *ret_size = bytes + sizeof(WCHAR);
    return text;
}

/* UTF-16 CF_UNICODETEXT -> UTF-8 with LF line endings, NUL-terminated */
static char *uni_to_lf_utf8( const WCHAR *src, size_t size )
{
    char *utf8;
    DWORD utf8_len;
    size_t i, j;

    if (size >= sizeof(WCHAR) && src[size / sizeof(WCHAR) - 1] == 0)
        size -= sizeof(WCHAR);

    RtlUnicodeToUTF8N(NULL, 0, &utf8_len, src, size);
    if (!(utf8 = malloc(utf8_len + 1))) return NULL;
    RtlUnicodeToUTF8N(utf8, utf8_len, &utf8_len, src, size);
    for (i = j = 0; i < utf8_len; i++)
    {
        if (utf8[i] == '\r' && (i + 1 >= utf8_len || utf8[i + 1] == '\n')) continue;
        utf8[j++] = utf8[i];
    }
    utf8[j] = 0;
    return utf8;
}

/* wrap an UTF-8 html fragment into the CF_HTML "HTML Format" layout;
 * returns malloc'd buffer, *ret_size = byte size */
static char *build_cf_html( const char *fragment )
{
    static const char header[] =
        "Version:0.9\n"
        "StartHTML:0000000100\n"
        "EndHTML:%010lu\n"
        "StartFragment:%010lu\n"
        "EndFragment:%010lu\n"
        "<!--StartFragment-->";
    static const char trailer[] = "\n<!--EndFragment-->";
    size_t frag_len = strlen(fragment);
    size_t head_len = strlen(header) + 12;   /* extra chars from %010lu */
    size_t total = head_len + frag_len + sizeof(trailer);
    char *ret;

    if (!(ret = malloc(total))) return NULL;
    sprintf(ret, header, (unsigned long)(total - 1), (unsigned long)head_len,
            (unsigned long)(head_len + frag_len + 1));
    memcpy(ret + head_len, fragment, frag_len);
    strcpy(ret + head_len + frag_len, trailer);
    return ret;
}

/* extract the UTF-8 fragment out of a CF_HTML "HTML Format" blob */
static char *parse_cf_html( const char *data, size_t size )
{
    const char *p = data;
    const char *end = data + size;
    const char *frag_start = NULL, *frag_end = NULL;
    const char *body = NULL;
    char *ret;
    size_t len;

    while (p < end && *p && *p != '<')
    {
        if (!strncmp(p, "StartFragment:", 14)) frag_start = data + atoi(p + 14);
        else if (!strncmp(p, "EndFragment:", 12)) frag_end = data + atoi(p + 12);
        if (!(p = strpbrk(p, "\r\n"))) break;
        while (*p == '\r' || *p == '\n') p++;
        if (p >= end) break;
    }

    if (frag_start && frag_end && frag_start < frag_end && frag_end <= end)
    {
        len = frag_end - frag_start;
        if ((ret = malloc(len + 1)))
        {
            memcpy(ret, frag_start, len);
            ret[len] = 0;
        }
        return ret;
    }

    /* no fragment markers: use the html body if present */
    if (p && p < end && *p == '<') body = p;
    if (!body) return NULL;
    len = end - body;
    if (!(ret = malloc(len + 1))) return NULL;
    memcpy(ret, body, len);
    ret[len] = 0;
    return ret;
}

/**************************************************************************
 *              get_nt_pathname / get_unix_file_name  (DOS -> unix)
 *
 * Ported from winex11.drv/clipboard.c for CF_HDROP export.
 */
static BOOL get_nt_pathname( const WCHAR *name, UNICODE_STRING *nt_name )
{
    static const WCHAR ntprefixW[] = {'\\','?','?','\\'};
    static const WCHAR uncprefixW[] = {'U','N','C','\\'};
    size_t len = wcslen(name);
    WCHAR *ptr;

    nt_name->MaximumLength = (len + 8) * sizeof(WCHAR);
    if (!(ptr = malloc(nt_name->MaximumLength))) return FALSE;
    nt_name->Buffer = ptr;

    memcpy(ptr, ntprefixW, sizeof(ntprefixW));
    ptr += ARRAY_SIZE(ntprefixW);
    if (name[0] == '\\' && name[1] == '\\')
    {
        if ((name[2] == '.' || name[2] == '?') && name[3] == '\\')
        {
            name += 4;
            len -= 4;
        }
        else
        {
            memcpy(ptr, uncprefixW, sizeof(uncprefixW));
            ptr += ARRAY_SIZE(uncprefixW);
            name += 2;
            len -= 2;
        }
    }
    memcpy(ptr, name, (len + 1) * sizeof(WCHAR));
    nt_name->Length = ((ptr - nt_name->Buffer) + len) * sizeof(WCHAR);
    return TRUE;
}

static char *get_unix_file_name( const WCHAR *dos_name )
{
    UNICODE_STRING nt_name;
    OBJECT_ATTRIBUTES attr;
    NTSTATUS status;
    ULONG size = 256;
    char *buffer;

    if (!get_nt_pathname(dos_name, &nt_name)) return NULL;
    InitializeObjectAttributes(&attr, &nt_name, 0, 0, NULL);
    for (;;)
    {
        if (!(buffer = malloc(size)))
        {
            free(nt_name.Buffer);
            return NULL;
        }
        status = wine_nt_to_unix_file_name(&attr, buffer, &size, FILE_OPEN_IF);
        if (status != STATUS_BUFFER_TOO_SMALL) break;
        free(buffer);
    }
    free(nt_name.Buffer);
    if (status)
    {
        free(buffer);
        return NULL;
    }
    return buffer;
}

/* percent-decode a URI and turn it into a DOS path; ported from winex11 */
static WCHAR *get_dos_path_from_unix( const char *unix_path );
static WCHAR *uri_to_dos( const char *encoded_uri )
{
    WCHAR *ret = NULL;
    size_t i, j = 0, len = strlen(encoded_uri);
    char *uri = calloc(len + 1, 1);

    if (!uri) return NULL;
    for (i = 0; encoded_uri[i]; i++)
    {
        if (encoded_uri[i] == '%')
        {
            if (encoded_uri[i + 1] && encoded_uri[i + 2])
            {
                char hex[3];
                hex[0] = encoded_uri[i + 1];
                hex[1] = encoded_uri[i + 2];
                hex[2] = 0;
                uri[j++] = (char)strtoul(hex, NULL, 16);
                i += 2;
            }
            else
            {
                WARN("invalid URI encoding in %s\n", debugstr_a(encoded_uri));
                free(uri);
                return NULL;
            }
        }
        else uri[j++] = encoded_uri[i];
    }

    if (!strncmp(uri, "file:///", 8))          ret = get_dos_path_from_unix(&uri[7]);
    else if (!strncmp(uri, "file://", 7))      ret = get_dos_path_from_unix(strchr(uri + 7, '/'));
    else if (!strncmp(uri, "file:/", 6))       ret = get_dos_path_from_unix(&uri[5]);
    free(uri);
    return ret;
}

/* unix char* path -> DOS WCHAR* path via ntdll */
static WCHAR *get_dos_path_from_unix( const char *unix_path )
{
    WCHAR *dos = NULL;

    if (!unix_path || !*unix_path) return NULL;
    if (ntdll_get_dos_file_name( unix_path, &dos, FILE_OPEN ) && !dos) return NULL;
    return dos;
}

/* CF_HDROP data -> "file://...\r\n" uri-list (malloc'd, NUL-terminated) */
static char *hdrop_to_uri_list( const DROPFILES *drop )
{
    char *list = NULL;
    size_t len = 0;
    const WCHAR *ptr;
    static const char hex[] = "0123456789abcdef";

    if (!drop->fWide)
    {
        WARN("CF_HDROP with ANSI filenames not supported\n");
        return NULL;
    }
    ptr = (const WCHAR *)((const char *)drop + drop->pFiles);

    while (*ptr)
    {
        char *unix_name = get_unix_file_name(ptr);
        size_t uri_size, u;
        char *new_list;

        ptr += wcslen(ptr) + 1;
        if (!unix_name) continue;

        uri_size = 8 + 3 * strlen(unix_name) + 3;
        if ((new_list = realloc(list, len + uri_size + 1)))
        {
            list = new_list;
            strcpy(list + len, "file:///");
            len += 8;
            for (u = 1; unix_name[u]; u++)   /* keep leading '/' */
            {
                list[len++] = '%';
                list[len++] = hex[(unsigned char)unix_name[u] >> 4];
                list[len++] = hex[(unsigned char)unix_name[u] & 0xf];
            }
            list[len++] = '\r';
            list[len++] = '\n';
        }
        free(unix_name);
    }

    if (list) list[len] = 0;
    return list;
}

/* "file://...\r\n" uri-list -> CF_HDROP DROPFILES (malloc'd);
 * ported from winex11 uri_list_to_drop_files */
static DROPFILES *uri_list_to_drop_files( const char *uri_list, size_t *ret_size )
{
    size_t size = strlen(uri_list);
    WCHAR *out, *path;
    char *uri;
    int total = 0, capacity = 4096, start = 0, end = 0;
    DROPFILES *drop_files = NULL;

    if (!(out = malloc(capacity * sizeof(WCHAR)))) return NULL;

    while (end < size)
    {
        while (end < size && uri_list[end] != '\r') end++;
        if (end < (size - 1) && uri_list[end + 1] != '\n')
        {
            WARN("URI list line doesn't end in \\r\\n\n");
            break;
        }

        if (!(uri = malloc(end - start + 1))) break;
        memcpy(uri, &uri_list[start], end - start);
        uri[end - start] = 0;
        path = uri_to_dos(uri);
        free(uri);

        if (path)
        {
            int path_size = wcslen(path) + 1;
            if (path_size > capacity - total)
            {
                WCHAR *new_out;
                capacity = 2 * capacity + path_size;
                new_out = realloc(out, (capacity + 1) * sizeof(WCHAR));
                if (!new_out) { free(path); break; }
                out = new_out;
            }
            memcpy(&out[total], path, path_size * sizeof(WCHAR));
            total += path_size;
            free(path);
        }

        start = end + 2;
        end = start;
    }

    if (out && end >= size)
    {
        *ret_size = sizeof(DROPFILES) + (total + 1) * sizeof(WCHAR);
        if ((drop_files = malloc(*ret_size)))
        {
            drop_files->pFiles = sizeof(DROPFILES);
            drop_files->pt.x = 0;
            drop_files->pt.y = 0;
            drop_files->fNC = 0;
            drop_files->fWide = TRUE;
            out[total] = 0;
            memcpy((char *)drop_files + drop_files->pFiles, out, (total + 1) * sizeof(WCHAR));
        }
    }
    free(out);
    return drop_files;
}

/* packed 24/32bpp DIB -> RGBA_8888 clip_image; NULL for other formats */
static struct clip_image *dib_to_image( const BITMAPINFO *bmi, size_t size )
{
    struct clip_image *image;
    int width, height, bpp, top_down;
    LONG src_stride;
    const BYTE *src_bits;
    BYTE *dst;
    UINT x, y;

    if (size < sizeof(BITMAPINFOHEADER)) return NULL;
    if (bmi->bmiHeader.biCompression != BI_RGB) return NULL;

    width = bmi->bmiHeader.biWidth;
    height = bmi->bmiHeader.biHeight;
    top_down = height < 0;
    if (top_down) height = -height;
    bpp = bmi->bmiHeader.biBitCount;
    if (bpp != 24 && bpp != 32) return NULL;
    if (width <= 0 || height <= 0) return NULL;

    src_stride = ((width * bpp + 31) / 32) * 4;
    if (size < sizeof(BITMAPINFOHEADER) + src_stride * height) return NULL;
    src_bits = (const BYTE *)bmi + sizeof(BITMAPINFOHEADER);

    if (!(image = malloc(sizeof(*image)))) return NULL;
    image->width = width;
    image->height = height;
    if (!(image->bits = malloc((size_t)width * height * 4)))
    {
        free(image);
        return NULL;
    }

    dst = image->bits;
    for (y = 0; y < height; y++)
    {
        const BYTE *src_row = src_bits + (top_down ? y : (height - 1 - y)) * src_stride;
        BYTE *dst_row = dst + y * width * 4;
        for (x = 0; x < width; x++)
        {
            dst_row[x * 4 + 0] = src_row[x * (bpp / 8) + 2];        /* R */
            dst_row[x * 4 + 1] = src_row[x * (bpp / 8) + 1];        /* G */
            dst_row[x * 4 + 2] = src_row[x * (bpp / 8) + 0];        /* B */
            dst_row[x * 4 + 3] = (bpp == 32) ? src_row[x * 4 + 3] : 0xFF; /* A */
        }
    }
    return image;
}

/* RGBA_8888 clip_image -> packed 32bpp top-down DIB (malloc'd) */
static BITMAPINFO *image_to_dib( const struct clip_image *image, size_t *ret_size )
{
    BITMAPINFO *bmi;
    BYTE *bits;
    size_t i, count;
    UINT stride;

    stride = image->width * 4;
    count = (size_t)stride * image->height;
    *ret_size = sizeof(BITMAPINFOHEADER) + count;
    if (!(bmi = malloc(*ret_size))) return NULL;

    memset(&bmi->bmiHeader, 0, sizeof(bmi->bmiHeader));
    bmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi->bmiHeader.biWidth = image->width;
    bmi->bmiHeader.biHeight = -(LONG)image->height;   /* top-down */
    bmi->bmiHeader.biPlanes = 1;
    bmi->bmiHeader.biBitCount = 32;
    bmi->bmiHeader.biCompression = BI_RGB;
    bmi->bmiHeader.biSizeImage = count;

    bits = (BYTE *)bmi + sizeof(BITMAPINFOHEADER);
    memcpy(bits, image->bits, count);
    for (i = 0; i < count; i += 4)   /* RGBA -> BGRA */
    {
        BYTE r = bits[i];
        bits[i] = bits[i + 2];
        bits[i + 2] = r;
    }
    return bmi;
}


/**************************************************************************
 *              materialize_import
 *
 * Store the prefetched OHOS content as real clipboard data (all formats
 * at once) through an ownerless clipboard.  Runs on a wine thread.
 */
static void materialize_import( struct pending_clip *in )
{
    if (!NtUserOpenClipboard(NULL, 0))
    {
        WARN("DIAGCLIP materialize: could not open win32 clipboard\n");
        return;
    }

    NtUserEmptyClipboard();

    if (in->text)
    {
        DWORD bytes;
        WCHAR *text = utf8_to_crlf_uni(in->text, &bytes);
        if (text)
        {
            struct set_clipboard_params params = { 0 };
            params.data = text;
            params.size = bytes;
            if (NtUserSetClipboardData(CF_UNICODETEXT, 0, &params))
                WARN("DIAGCLIP materialize: text %lu bytes\n", (unsigned long)bytes);
            free(text);
        }
    }

    if (in->html)
    {
        char *cf_html = build_cf_html(in->html);
        if (cf_html)
        {
            struct set_clipboard_params params = { 0 };
            UINT id = html_format_id();
            params.data = cf_html;
            params.size = strlen(cf_html) + 1;
            if (id && NtUserSetClipboardData(id, 0, &params))
                WARN("DIAGCLIP materialize: html %lu bytes\n", (unsigned long)params.size);
            free(cf_html);
        }
    }

    if (in->urilist)
    {
        size_t size;
        DROPFILES *drop = uri_list_to_drop_files(in->urilist, &size);
        if (drop)
        {
            struct set_clipboard_params params = { 0 };
            params.data = drop;
            params.size = size;
            if (NtUserSetClipboardData(CF_HDROP, 0, &params))
                WARN("DIAGCLIP materialize: hdrop %lu bytes\n", (unsigned long)size);
            free(drop);
        }
    }

    if (in->image)
    {
        size_t size;
        BITMAPINFO *bmi = image_to_dib(in->image, &size);
        if (bmi)
        {
            struct set_clipboard_params params = { 0 };
            params.data = bmi;
            params.size = size;
            if (NtUserSetClipboardData(CF_DIB, 0, &params))
                WARN("DIAGCLIP materialize: dib %ux%u\n", in->image->width, in->image->height);
            free(bmi);
        }
    }

    NtUserCloseClipboard();
}


/**************************************************************************
 *              export_win32_to_pasteboard
 *
 * If a win32 window (e.g. notepad) published new clipboard data, read
 * every supported format and hand it to the pbs worker.  Runs on a wine
 * thread.  Our own imports use an ownerless clipboard (owner == NULL)
 * and don't end up here.
 */
static void export_win32_to_pasteboard(void)
{
    struct pending_clip exp;
    DWORD seq = NtUserGetClipboardSequenceNumber();
    UINT html_id = html_format_id();

    if (seq == last_win32_seq) return;
    last_win32_seq = seq;

    if (!NtUserGetClipboardOwner()) return;  /* ownerless: our own import */

    memset(&exp, 0, sizeof(exp));

    if (!NtUserOpenClipboard(NULL, 0)) return;

    /* CF_UNICODETEXT */
    if (NtUserIsClipboardFormatAvailable(CF_UNICODETEXT))
    {
        size_t size;
        WCHAR *data = read_clipboard_data(CF_UNICODETEXT, &size);
        if (data)
        {
            exp.text = uni_to_lf_utf8(data, size);
            free(data);
        }
    }

    /* "HTML Format" */
    if (html_id && NtUserIsClipboardFormatAvailable(html_id))
    {
        size_t size;
        char *data = read_clipboard_data(html_id, &size);
        if (data)
        {
            exp.html = parse_cf_html(data, size);
            free(data);
        }
    }

    /* CF_HDROP */
    if (NtUserIsClipboardFormatAvailable(CF_HDROP))
    {
        size_t size;
        DROPFILES *data = read_clipboard_data(CF_HDROP, &size);
        if (data)
        {
            if (size >= sizeof(DROPFILES)) exp.urilist = hdrop_to_uri_list(data);
            free(data);
        }
    }

    /* CF_DIB */
    if (NtUserIsClipboardFormatAvailable(CF_DIB))
    {
        size_t size;
        BITMAPINFO *data = read_clipboard_data(CF_DIB, &size);
        if (data)
        {
            exp.image = dib_to_image(data, size);
            free(data);
        }
    }

    NtUserCloseClipboard();

    if (exp.text || exp.html || exp.urilist || exp.image)
    {
        WARN("DIAGCLIP export: text=%d html=%d hdrop=%d dib=%d\n",
             exp.text ? 1 : 0, exp.html ? 1 : 0, exp.urilist ? 1 : 0, exp.image ? 1 : 0);
        put_export_pending(&exp);
    }
}


/**************************************************************************
 *              clipboard_sync
 *
 * One sync round on a wine thread: materialize any prefetched OHOS data,
 * then check the win32 side for new data to export.  Never blocks on
 * binder IPC (only shared-buffer take/put).
 *
 * win32u calls pUpdateClipboard (-> OHOS_UpdateClipboard) from inside
 * NtUserOpenClipboard/EmptyClipboard/etc., and our own NtUser calls can
 * re-enter those -- a same-thread re-entry is simply skipped.
 */
static void clipboard_sync(void)
{
    static __thread int in_sync;
    struct pending_clip imp;

    if (in_sync) return;

    in_sync = 1;

    /* A win32 app is reading the clipboard while the last OHOS import was
     * rejected (READ_PASTEBOARD not granted): have the pbs worker run the
     * runtime permission request, so the dialog pops at paste time with the
     * wine app in the foreground.  Skip when a win32 app owns the clipboard
     * (the user just copied inside the win32 app, no import needed). */
    if (InterlockedCompareExchange(&import_denied, 0, 0) &&
        !InterlockedCompareExchange(&perm_denied_by_user, 0, 0) &&
        !InterlockedCompareExchange(&perm_inflight, 0, 0) &&
        !NtUserGetClipboardOwner())
    {
        InterlockedExchange(&perm_request_pending, 1);
    }

    imp = take_import_pending();
    if (imp.text || imp.html || imp.urilist || imp.image)
    {
        materialize_import(&imp);
        free_pending_clip(&imp);
    }

    export_win32_to_pasteboard();

    in_sync = 0;
}


 /**************************************************************************
  *              OHOS User Driver Clipboard Exports
  **************************************************************************/

/**************************************************************************
 *              init_ohos_pasteboard
 *
 * Start the pbs worker thread. Called from ohos_init() during driver
 * startup.  All OH_Pasteboard object creation happens on the worker
 * itself, keeping binder usage single-threaded.
 */
BOOL init_ohos_pasteboard(void)
{
    if (InterlockedCompareExchange(&pbs_thread_started, 1, 0)) return TRUE;

    if (pthread_create(&pbs_thread, NULL, pbs_worker, NULL) != 0)
    {
        ERR("DIAGCLIP failed to create pbs worker thread\n");
        InterlockedExchange(&pbs_thread_started, 0);
        return FALSE;
    }
    return TRUE;
}


/**************************************************************************
 *              OHOS_ClipboardWindowProc
 *
 * Window procedure of the per-process "__wineohos_clipboard_manager"
 * message-only window created by the PE stub's clipboard thread.  The
 * thread pumps messages, so this runs as a proper wine thread: NtUser
 * calls are safe here, binder calls are not (use the shared buffers).
 */
LRESULT OHOS_ClipboardWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
    case WM_NCCREATE:
        /* capture the current sequence number so we don't export whatever
         * happens to be in the win32 clipboard before the window existed */
        last_win32_seq = NtUserGetClipboardSequenceNumber();
        NtUserAddClipboardFormatListener(hwnd);
        WARN("DIAGCLIP clipboard listener registered hwnd=%p seq=%lu\n",
             hwnd, (unsigned long)last_win32_seq);
        /* initial sync round: materialize any startup pasteboard content */
        clipboard_sync();
        return TRUE;
    case WM_CLIPBOARDUPDATE:
        /* a win32 app published new clipboard data -- push it to OHOS.
         * (This is the only reliable export trigger: the desktop-window
         * hook never receives WM_CLIPBOARDUPDATE because win32u only
         * forwards msg >= WM_USER to pDesktopWindowProc.) */
        clipboard_sync();
        break;
    }
    return 0;
}


/**************************************************************************
 *              OHOS_UpdateClipboard
 *
 * Called from the Win32 side whenever apps query the clipboard
 * (OpenClipboard / IsClipboardFormatAvailable / ...).  Throttled, then
 * one sync round -- this is what materializes OHOS data right before
 * notepad reads it.
 */
void OHOS_UpdateClipboard(void)
{
    static ULONG last_sync_tick;
    ULONG now;

    now = NtGetTickCount();
    if (last_sync_tick && (int)(now - last_sync_tick) < CLIPBOARD_SYNC_MIN_INTERVAL_MS) return;
    last_sync_tick = now;

    clipboard_sync();
}
