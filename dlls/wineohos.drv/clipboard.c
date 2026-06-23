/*
 * OHOS clipboard driver
 */

#if 0
#pragma makedep unix
#endif

#include "config.h"

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include <database/pasteboard/oh_pasteboard.h>
#include "windef.h"
#include "winuser.h"
#include "ntuser.h"
// #include "shellapi.h"
// #include "shlobj.h"
#include "wine/list.h"
// #include "wine/server.h"
// #include "ohos.h"
#include "wine/debug.h"



WINE_DEFAULT_DEBUG_CHANNEL(ohos);
/**************************************************************************
 *              Types
 **************************************************************************/


typedef void *(*DRVIMPORTFUNC)(OH_UdmfData* data, size_t *ret_size);
typedef OH_UdmfData* (*DRVEXPORTFUNC)(void *data, size_t size);
typedef const char* OHOS_PasteType; // OHOS MIME 类型

typedef struct _WINE_CLIPFORMAT
{
    struct list             entry;
    UINT                    format_id;
    OHOS_PasteType          type;
    DRVIMPORTFUNC           import_func;
    DRVEXPORTFUNC           export_func;
    BOOL                    synthesized;
    struct _WINE_CLIPFORMAT *natural_format;
} WINE_CLIPFORMAT;


/**************************************************************************
 *              Constants
 **************************************************************************/

#define CLIPBOARD_UPDATE_DELAY 2000   /* delay between checks of the Mac pasteboard */


 /**************************************************************************
 *              Forward Function Declarations
 **************************************************************************/
static void *import_utf8_to_unicodetext(OH_UdmfData* data, size_t *ret_size);
static OH_UdmfData* export_unicodetext_to_utf8(void *data, size_t size);
static WINE_CLIPFORMAT* format_for_type(const char *type);
static WINE_CLIPFORMAT* natural_format_for_format(UINT format_id);


 /**************************************************************************
 *              Static Variables
 **************************************************************************/

static const WCHAR clipboard_classname[] =
    {'_','_','w','i','n','e','_','c','l','i','p','b','o','a','r','d','_','m','a','n','a','g','e','r',0};
/* Clipboard formats */
static struct list format_list = LIST_INIT(format_list);

static HWND clipboard_hwnd;
static BOOL is_clipboard_owner;
static volatile LONG pasteboard_notify_flag;   /* set by observer callback, cleared by clipboard thread */
static volatile LONG exporting_to_ohos;        /* guard: TRUE while Win32->OHOS push is in flight */
static OH_Pasteboard* OHOS_pasteboard;
static OH_PasteboardObserver* OHOS_observer;
static WINE_CLIPFORMAT **current_ohos_formats;
static unsigned int nb_current_ohos_formats;

static const struct
{
    UINT            id;
    OHOS_PasteType  type;
    DRVIMPORTFUNC   import;
    DRVEXPORTFUNC   export;
    BOOL            synthesized;
} builtin_format_ids[] =
{
    // 使用 UTF-8 作为 "text/plain" 的存储格式，映射到 CF_UNICODETEXT
    { CF_UNICODETEXT, "text/plain", import_utf8_to_unicodetext, export_unicodetext_to_utf8, FALSE },
};

static unsigned int clipboard_thread_id;
static unsigned int last_clipboard_update;

 /**************************************************************************
 *              Internal Clipboard implementation methods
 **************************************************************************/


/**************************************************************************
 *              debugstr_format
 */
static const char *debugstr_format(UINT id)
{
    WCHAR buffer[256];

    if (NtUserGetClipboardFormatName(id, buffer, 256))
        return wine_dbg_sprintf("0x%04x %s", id, debugstr_w(buffer));

    switch (id)
    {
#define BUILTIN(id) case id: return #id;
    BUILTIN(CF_TEXT)
    BUILTIN(CF_BITMAP)
    BUILTIN(CF_METAFILEPICT)
    BUILTIN(CF_SYLK)
    BUILTIN(CF_DIF)
    BUILTIN(CF_TIFF)
    BUILTIN(CF_OEMTEXT)
    BUILTIN(CF_DIB)
    BUILTIN(CF_PALETTE)
    BUILTIN(CF_PENDATA)
    BUILTIN(CF_RIFF)
    BUILTIN(CF_WAVE)
    BUILTIN(CF_UNICODETEXT)
    BUILTIN(CF_ENHMETAFILE)
    BUILTIN(CF_HDROP)
    BUILTIN(CF_LOCALE)
    BUILTIN(CF_DIBV5)
    BUILTIN(CF_OWNERDISPLAY)
    BUILTIN(CF_DSPTEXT)
    BUILTIN(CF_DSPBITMAP)
    BUILTIN(CF_DSPMETAFILEPICT)
    BUILTIN(CF_DSPENHMETAFILE)
#undef BUILTIN
    default: return wine_dbg_sprintf("0x%04x", id);
    }
}


/**************************************************************************
 *              set_win32_clipboard_formats_from_ohos_pasteboard
 *
 * Given a list of MIME type strings from the OHOS pasteboard, look up
 * each one in format_list and set the corresponding Win32 clipboard format
 * via deferred rendering (WM_RENDERFORMAT).  The actual data retrieval
 * happens later in render_format().
 */
static void set_win32_clipboard_formats_from_ohos_pasteboard(char** types, int count)
{
    WINE_CLIPFORMAT **new_formats = NULL;
    UINT i, format_idx = 0;

    if (!types || count <= 0) return;

    new_formats = calloc(count, sizeof(WINE_CLIPFORMAT*));
    if (!new_formats) return;

    for (i = 0; i < count; i++)
    {
        struct set_clipboard_params params = { 0 };
        WINE_CLIPFORMAT *fmt = format_for_type(types[i]);

        if (!fmt)
        {
            TRACE("no handler for type %s, skipping\n", types[i]);
            continue;
        }

        TRACE("adding format %s\n", debugstr_format(fmt->format_id));
        NtUserSetClipboardData(fmt->format_id, 0, &params);
        new_formats[format_idx++] = fmt;
    }

    free(current_ohos_formats);
    current_ohos_formats = new_formats;
    nb_current_ohos_formats = format_idx;
}


/**************************************************************************
 *              render_format
 */
static void render_format(UINT id)
{
    unsigned int i;
    for (i = 0; i < nb_current_ohos_formats; i++)
    {
        if (current_ohos_formats[i]->format_id != id) continue;

        bool hasDataType = OH_Pasteboard_HasType(OHOS_pasteboard, current_ohos_formats[i]->type);
        if (hasDataType) {
            int ret = 0;
            OH_UdmfData* udmfData = OH_Pasteboard_GetData(OHOS_pasteboard, &ret);
            if (udmfData) {
                struct set_clipboard_params params = { 0 };
                params.data = current_ohos_formats[i]->import_func(udmfData, &params.size);
                if (!params.data) continue;
                NtUserSetClipboardData(id, 0, &params);
                free(params.data);
                break;
            }

        }
    }
}
 

/**************************************************************************
 *              grab_win32_clipboard
 *
 * Grab the Win32 clipboard when a OHOS app has taken ownership of the
 * pasteboard, and fill it with the pasteboard data types.
 * 同步数据.
 */
static void grab_win32_clipboard(void)
{
    static char** last_types;
    char** types;

    int ret = 0;
    unsigned int count = 0;
    OH_UdmfData* udmfData = OH_Pasteboard_GetData(OHOS_pasteboard, &ret);
    types = OH_UdmfData_GetTypes(udmfData, &count);

    if (!types)
    {
        WARN("Failed to copy pasteboard types\n");
        OH_UdmfData_Destroy(udmfData);
        return;
    }

    if (!NtUserOpenClipboard(clipboard_hwnd, 0))
    {
        OH_UdmfData_Destroy(udmfData);
        return;
    }
    NtUserEmptyClipboard();
    is_clipboard_owner = TRUE;
    last_clipboard_update = NtGetTickCount();
    set_win32_clipboard_formats_from_ohos_pasteboard(types, count);
    NtUserCloseClipboard();
    OH_UdmfData_Destroy(udmfData);
}

/**************************************************************************
 *              extract_plain_text_from_udmf
 *
 *  extract_plain_text_from_udmf.
 */
static char* extract_plain_text_from_udmf(OH_UdmfData* udmf_data, size_t* out_len)
{
    if (!udmf_data || !out_len) {
        *out_len = 0;
        return NULL;
    }

    // 1. 获取第一个 record（通常只有一个）
    OH_UdmfRecord* record = OH_UdmfData_GetRecord(udmf_data, 0);
    if (!record) {
        *out_len = 0;
        return NULL;
    }

    // 2. 创建 PlainText 解析器
    OH_UdsPlainText* plain_text = OH_UdsPlainText_Create();
    if (!plain_text) {
        OH_UdmfRecord_Destroy(record);
        *out_len = 0;
        return NULL;
    }

    // 3. 从 record 中提取文本
    int ret = OH_UdmfRecord_GetPlainText(record, plain_text);
    if (ret != 0) { // 假设非 0 表示失败（请根据实际 API 调整）
        OH_UdsPlainText_Destroy(plain_text);
        OH_UdmfRecord_Destroy(record);
        *out_len = 0;
        return NULL;
    }

    const char* content = OH_UdsPlainText_GetContent(plain_text);
    if (!content) {
        OH_UdsPlainText_Destroy(plain_text);
        OH_UdmfRecord_Destroy(record);
        *out_len = 0;
        return NULL;
    }

    size_t len = strlen(content);
    char* copied = malloc(len + 1);
    if (copied) {
        memcpy(copied, content, len + 1);
        *out_len = len;
    } else {
        *out_len = 0;
    }

    // 4. 清理临时对象
    OH_UdsPlainText_Destroy(plain_text);
    OH_UdmfRecord_Destroy(record);

    return copied; // caller 必须 free()
}


/**************************************************************************
 *              create_udmf_data_from_utf8_text
 *
 *  create_udmf_data_from_utf8_text.
 */
OH_UdmfData* create_udmf_data_from_utf8_text(const char* utf8_text)
{
    if (!utf8_text) {
        return NULL;
    }

    int ret;

    // 1. 创建 PlainText 对象
    OH_UdsPlainText* plain_text = OH_UdsPlainText_Create();
    if (!plain_text) {
        return NULL;
    }
    OH_UdsPlainText_SetContent(plain_text, utf8_text);

    // 2. 创建 Record 并添加文本
    OH_UdmfRecord* record = OH_UdmfRecord_Create();
    if (!record) {
        OH_UdsPlainText_Destroy(plain_text);
        return NULL;
    }
    ret = OH_UdmfRecord_AddPlainText(record, plain_text);

    OH_UdsPlainText_Destroy(plain_text); // Add 后通常可释放

    if (ret != 0) {
        OH_UdmfRecord_Destroy(record);
        return NULL;
    }

    // 3. 创建 UDMF Data 并添加 Record
    OH_UdmfData* udmf_data = OH_UdmfData_Create();
    if (!udmf_data) {
        OH_UdmfRecord_Destroy(record);
        return NULL;
    }
    ret = OH_UdmfData_AddRecord(udmf_data, record);
    OH_UdmfRecord_Destroy(record); // Add 后可释放

    if (ret != 0) {
        OH_UdmfData_Destroy(udmf_data);
        return NULL;
    }

    return udmf_data; // caller 负责 OH_UdmfData_Destroy
}


/**************************************************************************
 *              import_utf8_to_unicodetext
 *
 *  Import a UTF-8 string, converting the string to CF_UNICODETEXT.
 */
static void *import_utf8_to_unicodetext(OH_UdmfData* data, size_t *ret_size)
{
    if (!data || !ret_size) {
        if (ret_size) *ret_size = 0;
        return NULL;
    }

    // Step 1: 从 UDMF 数据中提取 UTF-8 文本（会分配内存）
    size_t src_len;
    char *src_utf8 = extract_plain_text_from_udmf(data, &src_len);
    if (!src_utf8 || src_len == 0) {
        *ret_size = 0;
        return NULL;
    }

    // Step 2: 统计有多少个 '\n'（用于转换为 \r\n）
    unsigned long new_lines = 0;
    for (size_t i = 0; i < src_len; i++) {
        if (src_utf8[i] == '\n')
            new_lines++;
    }

    // Step 3: 分配缓冲区，将 \n → \r\n
    char *crlf_buffer = malloc(src_len + new_lines + 1);
    if (!crlf_buffer) {
        free(src_utf8);
        *ret_size = 0;
        return NULL;
    }

    size_t j = 0;
    for (size_t i = 0; i < src_len; i++) {
        if (src_utf8[i] == '\n') {
            crlf_buffer[j++] = '\r';
        }
        crlf_buffer[j++] = src_utf8[i];
    }
    crlf_buffer[j] = '\0'; // 确保以 null 结尾

    // Step 4: 将 UTF-8 (with CRLF) 转为 UTF-16
    WCHAR *unicode_text = malloc((j + 1) * sizeof(WCHAR));
    if (!unicode_text) {
        free(crlf_buffer);
        free(src_utf8);
        *ret_size = 0;
        return NULL;
    }

    DWORD unicode_byte_size;
    RtlUTF8ToUnicodeN(unicode_text, (j + 1) * sizeof(WCHAR), &unicode_byte_size, crlf_buffer, j);
    *ret_size = unicode_byte_size;

    // Step 5: 清理临时内存
    free(crlf_buffer);
    free(src_utf8); // ← 别忘了释放 extract 函数返回的内存！

    return unicode_text; // 返回 UTF-16 字符串（caller 用完需 GlobalFree 或类似？但 Wine 通常由上层管理）
}


/**************************************************************************
 *              export_unicodetext_to_utf8
 *
 *  Export CF_UNICODETEXT to UTF-8.
 */
static OH_UdmfData* export_unicodetext_to_utf8(void *data, size_t size)
{
    if (!data || size == 0) {
        return NULL;
    }

    WCHAR *src = (WCHAR*)data;

    // Step 1: 去掉末尾 null（如果存在）
    if (size >= sizeof(WCHAR) && src[size / sizeof(WCHAR) - 1] == 0) {
        size -= sizeof(WCHAR);
    }

    // Step 2: 计算 UTF-8 所需长度
    DWORD utf8_len = 0;
    RtlUnicodeToUTF8N(NULL, 0, &utf8_len, src, size);

    // Step 3: 分配缓冲区并转换
    char *utf8_buffer = malloc(utf8_len + 1);
    if (!utf8_buffer) {
        return NULL;
    }

    RtlUnicodeToUTF8N(utf8_buffer, utf8_len, &utf8_len, src, size);
    utf8_buffer[utf8_len] = '\0'; // 确保以 null 结尾（方便后续处理）

    // Step 4: 移除 \r（将 \r\n → \n）
    size_t j = 0;
    for (size_t i = 0; i < utf8_len; i++) {
        if (utf8_buffer[i] == '\r' &&
            (i + 1 >= utf8_len || utf8_buffer[i + 1] == '\n')) {
            continue;
        }
        utf8_buffer[j++] = utf8_buffer[i];
    }
    utf8_buffer[j] = '\0'; // 新字符串结束

    // Step 5: 封装为 OH_UdmfData
    OH_UdmfData* result = create_udmf_data_from_utf8_text(utf8_buffer);

    free(utf8_buffer);
    return result; // 返回给上层，由它调用 OH_Pasteboard_SetData
}


/**************************************************************************
 *              PasteboardNotifyImpl
 *
 * PasteboardNotifyImpl.
 * 定义剪贴板数据内容变更时的通知回调函数
 */
static void PasteboardNotifyImpl(void* context, Pasteboard_NotifyType type)
{
    TRACE("Pasteboard_NotifyType, type: %d\n", type);

    /* If we are currently exporting data from Win32 to OHOS,
     * ignore the notification to avoid a feedback loop. */
    if (InterlockedCompareExchange(&exporting_to_ohos, 0, 0)) return;

    /* Signal the clipboard thread to pull OHOS data into Win32 */
    InterlockedExchange(&pasteboard_notify_flag, 1);
}


/**************************************************************************
 *              PasteboardFinalizeImpl
 *
 * PasteboardFinalizeImpl.
 * 定义剪贴板数据变更观察者对象销毁时的通知回调函数
 */
static void PasteboardFinalizeImpl(void* context)
{
    TRACE("callback: Pasteboard_Finalize");
}


/**************************************************************************
 *              natural_format_for_format
 *
 * Find the "natural" format for this format_id (the one which isn't
 * synthesized from another type).
 */
static WINE_CLIPFORMAT* natural_format_for_format(UINT format_id)
{
    WINE_CLIPFORMAT *format;

    LIST_FOR_EACH_ENTRY(format, &format_list, WINE_CLIPFORMAT, entry)
        if (format->format_id == format_id && !format->synthesized) break;

    if (&format->entry == &format_list)
        format = NULL;

    TRACE("%s -> %p/%s\n", debugstr_format(format_id), format, format ? format->type : NULL);
    return format;
}


/**************************************************************************
 *              register_builtin_formats
 * register_builtin_formats.
 * 对齐数据类型
 */
static void register_builtin_formats(void)
{
    UINT i;
    WINE_CLIPFORMAT *format;

    /* Register built-in formats */
    for (i = 0; i < ARRAY_SIZE(builtin_format_ids); i++)
    {
        if (!(format = malloc(sizeof(*format)))) break;
        format->format_id       = builtin_format_ids[i].id;
        format->type            = builtin_format_ids[i].type;
        format->import_func     = builtin_format_ids[i].import;
        format->export_func     = builtin_format_ids[i].export;
        format->synthesized     = builtin_format_ids[i].synthesized;
        format->natural_format  = NULL;
        list_add_tail(&format_list, &format->entry);
    }


    LIST_FOR_EACH_ENTRY(format, &format_list, WINE_CLIPFORMAT, entry)
    {
        if (format->synthesized)
            format->natural_format = natural_format_for_format(format->format_id);
    }
}


/**************************************************************************
 *              format_for_type
 */
static WINE_CLIPFORMAT* format_for_type(const char *type)
{
    WINE_CLIPFORMAT *format;

    if (list_empty(&format_list))
        register_builtin_formats();

    LIST_FOR_EACH_ENTRY(format, &format_list, WINE_CLIPFORMAT, entry)
    {
        if (!strcmp(format->type, type))
            return format;
    }

    return NULL;
}



/**************************************************************************
 *              update_clipboard
 *
 * Periodically update the clipboard while the clipboard is owned by a
 * OHOS app.
 */
static void update_clipboard(void)
{
    static BOOL updating;

    TRACE("is_clipboard_owner %d last_clipboard_update %u now %u\n",
          is_clipboard_owner, last_clipboard_update, NtGetTickCount());

    if (updating) return;
    updating = TRUE;

    if (is_clipboard_owner)
    {
        if (NtGetTickCount() - last_clipboard_update > CLIPBOARD_UPDATE_DELAY)
            grab_win32_clipboard();
    }
    else if (InterlockedExchange(&pasteboard_notify_flag, 0))
    {
        /* external OHOS app changed the pasteboard -- pull data into Win32 */
        grab_win32_clipboard();
    }

    updating = FALSE;
}


/**************************************************************************
 *              init_ohos_pasteboard
 *
 * Create the OHOS pasteboard and observer on the calling thread.
 * Called from ohos_init() during driver startup (UI thread).
 */
BOOL init_ohos_pasteboard(void)
{
    int ret;

    if (OHOS_pasteboard) return TRUE;  /* already initialized */

    OHOS_pasteboard = OH_Pasteboard_Create();
    if (!OHOS_pasteboard)
    {
        ERR("Failed to create pasteboard instance\n");
        return FALSE;
    }

    OHOS_observer = OH_PasteboardObserver_Create();
    if (!OHOS_observer)
    {
        ERR("Failed to create pasteboard observer\n");
        OH_Pasteboard_Destroy(OHOS_pasteboard);
        OHOS_pasteboard = NULL;
        return FALSE;
    }

    ret = OH_PasteboardObserver_SetData(OHOS_observer, (void*)OHOS_pasteboard,
                                        PasteboardNotifyImpl, PasteboardFinalizeImpl);
    if (ret != 0)
    {
        ERR("Failed to set pasteboard observer data, ret=%d\n", ret);
        OH_PasteboardObserver_Destroy(OHOS_observer);
        OHOS_observer = NULL;
        OH_Pasteboard_Destroy(OHOS_pasteboard);
        OHOS_pasteboard = NULL;
        return FALSE;
    }

    TRACE("pasteboard and observer created\n");
    return TRUE;
}


/**************************************************************************
 *              cleanup_pasteboard
 *
 * Unsubscribe and destroy the OHOS pasteboard and observer.
 * Called from WM_DESTROY when the clipboard manager window is destroyed.
 */
static void cleanup_pasteboard(void)
{
    if (OHOS_pasteboard && OHOS_observer)
    {
        OH_Pasteboard_Unsubscribe(OHOS_pasteboard, NOTIFY_LOCAL_DATA_CHANGE, OHOS_observer);
        TRACE("pasteboard unsubscribed\n");
    }

    if (OHOS_observer)
    {
        OH_PasteboardObserver_Destroy(OHOS_observer);
        OHOS_observer = NULL;
    }

    if (OHOS_pasteboard)
    {
        OH_Pasteboard_Destroy(OHOS_pasteboard);
        OHOS_pasteboard = NULL;
    }

    TRACE("pasteboard cleaned up\n");
}


/**************************************************************************
 *              init_clipboard
 *
 * Initialize the clipboard manager window.  Called from WM_NCCREATE.
 * The pasteboard and observer must already exist from init_ohos_pasteboard().
 */
static BOOL init_clipboard(HWND hwnd)
{
    static BOOL initialized = FALSE;

    /* re-entrant guard: WM_NCCREATE may fire more than once */
    if (initialized) return TRUE;
    
    if (!OHOS_pasteboard)
    {
        ERR("pasteboard not initialized -- init_ohos_pasteboard() must be called first\n");
        return FALSE;
    }

    clipboard_hwnd = hwnd;
    clipboard_thread_id = GetCurrentThreadId();
    NtUserAddClipboardFormatListener(clipboard_hwnd);

    /* subscribe to OHOS pasteboard data change notifications */
    {
        int ret = OH_Pasteboard_Subscribe(OHOS_pasteboard, NOTIFY_LOCAL_DATA_CHANGE, OHOS_observer);
        if (ret != 0)
            WARN("Failed to subscribe to pasteboard notifications, ret=%d\n", ret);
    }

    /* register builtin format mappings (CF_UNICODETEXT <-> "text/plain") */
    register_builtin_formats();

    /* start periodic sync timer -- always on so we can detect external changes */
    NtUserSetTimer(clipboard_hwnd, 1, CLIPBOARD_UPDATE_DELAY, NULL, TIMERV_DEFAULT_COALESCING);

    /* initial sync: pull data from OHOS pasteboard into Win32 clipboard */
    grab_win32_clipboard();

    TRACE("clipboard thread %04x running\n", clipboard_thread_id);
    initialized = TRUE;
    return TRUE;
}


/**************************************************************************
 *              OHOS_ClipboardWindowProc
 *
 * Window procedure for the clipboard manager.
 * windows到OHoS的事件回调注册。
 */
LRESULT OHOS_ClipboardWindowProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg)
    {
        case WM_NCCREATE:
            return init_clipboard(hwnd);
        case WM_CLIPBOARDUPDATE:
        {
            /* Don't push our own changes back to OHOS (avoid echo loop). */
            if (is_clipboard_owner) break;
            /* Push Win32 clipboard data to OHOS pasteboard. */
            {
                UINT i;
                for (i = 0; i < ARRAY_SIZE(builtin_format_ids); i++)
                    query_pasteboard_data(hwnd, builtin_format_ids[i].type);
            }
            break;
        }
        case WM_RENDERFORMAT:
            render_format(wp);
            break;
        case WM_TIMER:
            update_clipboard();
            break;
        case WM_DESTROYCLIPBOARD:
            is_clipboard_owner = FALSE;
            break;
        case WM_DESTROY:
            NtUserKillTimer(hwnd, 1);
            cleanup_pasteboard();
            break;
        case WM_USER:
            update_clipboard();
            break;
    }
    return NtUserMessageCall(hwnd, msg, wp, lp, NULL, NtUserDefWindowProc, FALSE);
}




 /**************************************************************************
 *              OHOS User Driver Clipboard Exports
 **************************************************************************/


/**************************************************************************
 *              OHOS_UpdateClipboard
 */
void OHOS_UpdateClipboard(void)
{
    static ULONG last_update;
    static HWND clipboard_manager;
    ULONG now;
    DWORD_PTR ret;

    if (GetCurrentThreadId() == clipboard_thread_id) return;

    TRACE("\n");

    now = NtGetTickCount();
    if (last_update && (int)(now - last_update) <= CLIPBOARD_UPDATE_DELAY) return;

    if (!NtUserIsWindow(clipboard_manager))
    {
        UNICODE_STRING str = RTL_CONSTANT_STRING(clipboard_classname);
        clipboard_manager = NtUserFindWindowEx(NULL, NULL, &str, NULL, 0);
        if (!clipboard_manager)
        {
            ERR("clipboard manager not found\n");
            return;
        }
    }

    send_message_timeout(clipboard_manager, WM_USER, 0, 0,
                         SMTO_ABORTIFHUNG, 5000, &ret);
    last_update = now;
}


  /**************************************************************************
 *              OHOSDRV Private Clipboard Exports
 **************************************************************************/


 /**************************************************************************
 *              query_pasteboard_data
 *
 * Read Win32 clipboard for a given OHOS paste type and push it to the
 * OHOS pasteboard.  Used by WM_CLIPBOARDUPDATE handler.
 */
BOOL query_pasteboard_data(HWND hwnd, OHOS_PasteType type)
{
    struct get_clipboard_params params = { .data_only = TRUE, .size = 1024 };
    WINE_CLIPFORMAT *format;
    BOOL ret = FALSE;

    format = format_for_type(type);
    if (!format) return FALSE;

    if (!NtUserOpenClipboard(clipboard_hwnd, 0))
    {
        ERR("failed to open clipboard for %s\n", type);
        return FALSE;
    }

    for (;;)
    {
        if (!(params.data = malloc(params.size))) break;
        if (NtUserGetClipboardData(format->format_id, &params))
        {
            OH_UdmfData* data;

            TRACE("exporting %s\n", debugstr_format(format->format_id));

            if ((data = format->export_func(params.data, params.size)))
            {
                int ErrCode;

                /* Set guard before calling OH_Pasteboard_SetData so the
                 * observer callback knows to skip this change (we caused it). */
                InterlockedExchange(&exporting_to_ohos, 1);

                ErrCode = OH_Pasteboard_SetData(OHOS_pasteboard, data);

                /* Clear guard and any flag the observer may have set */
                InterlockedExchange(&exporting_to_ohos, 0);
                InterlockedExchange(&pasteboard_notify_flag, 0);

                if (ErrCode == 0)
                    ret = TRUE;
                else
                    WARN("OH_Pasteboard_SetData error %d\n", ErrCode);

                OH_UdmfData_Destroy(data);
            }
            free(params.data);
            break;
        }
        free(params.data);
        if (!params.data_size) break;
        params.size = params.data_size;
        params.data_size = 0;
    }

    NtUserCloseClipboard();

    return ret;
}
