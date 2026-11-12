/*
 * VK_LAYER_CODEX_probe - runtime Vulkan interface tracer for Wine OHOS.
 *
 * Attached implicitly via the HarmonyOS debug-layer parameters
 * (debug.graphic.debug_layer / debug.graphic.debug_hap, see LAUNCHER.md).
 * Because winevulkan resolves every function it needs through
 * vkGetInstanceProcAddr / vkGetDeviceProcAddr, and the loader walks the
 * layer chain for each core function while building dispatch tables,
 * logging name queries at this layer captures the complete set of
 * vk interfaces required at runtime by the running game.
 *
 * Recorded per process load:
 *   - process identity (pid / cmdline)
 *   - vkCreateInstance: application info, enabled layers/extensions
 *   - supported instance extensions (queried down the chain)
 *   - vkEnumerateDeviceExtensionProperties results (supported device exts)
 *   - vkCreateDevice: enabled extensions + pNext struct types (feature gates)
 *   - every vkGetInstanceProcAddr / vkGetDeviceProcAddr name + count
 *   - queries that resolved to NULL (= capability gaps on this device)
 *
 * Output: /data/storage/el2/base/haps/entry/files/vk-trace/trace-<pid>.log
 * plus a hilog mirror (tag CodexVkLayer) of key events and a compact
 * flush of the query table, so data can be collected off-device with a
 * streaming hilog capture (the file lives inside the app sandbox).
 */
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <hilog/log.h>
#include "vk_layer.h"

#define LOG(fmt, ...) OH_LOG_Print(LOG_APP, LOG_INFO, 0x0000, "CodexVkLayer", fmt, ##__VA_ARGS__)
#define LAYER_NAME "VK_LAYER_CODEX_probe"

#define TRACE_DIR  "/data/storage/el2/base/haps/entry/files/vk-trace"
#define NAME_MAX_LEN 56
#define TRACER_SLOTS 4096

static PFN_vkGetInstanceProcAddr next_gipa;
static PFN_vkGetDeviceProcAddr next_gdpa;
static pthread_mutex_t trace_lock = PTHREAD_MUTEX_INITIALIZER;
static FILE *tracef;
static VkInstance seen_instance;

struct trace_entry
{
    char name[NAME_MAX_LEN];
    uint32_t gipa;
    uint32_t gdpa;
};
static struct trace_entry trace_table[TRACER_SLOTS];
static uint32_t total_gipa, total_gdpa, missing_gipa, missing_gdpa;

/* ------------------------------------------------------------------ */
/* trace output                                                        */

static void trace_vline(const char *fmt, va_list ap)
{
    char buf[512];
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    if (n <= 0) return;
    LOG("%{public}s", buf);
    pthread_mutex_lock(&trace_lock);
    if (!tracef)
    {
        char path[128];
        mkdir(TRACE_DIR, 0700);
        snprintf(path, sizeof(path), TRACE_DIR "/trace-%d.log", (int)getpid());
        tracef = fopen(path, "a");
    }
    if (tracef)
    {
        fputs(buf, tracef);
        fputc('\n', tracef);
    }
    pthread_mutex_unlock(&trace_lock);
}

static void trace_line(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    trace_vline(fmt, ap);
    va_end(ap);
}

static void trace_header(void)
{
    char cmdline[256] = {0};
    FILE *f = fopen("/proc/self/cmdline", "r");
    if (f)
    {
        size_t n = fread(cmdline, 1, sizeof(cmdline) - 1, f);
        fclose(f);
        for (size_t i = 0; i + 1 < n; i++)
            if (!cmdline[i]) cmdline[i] = ' ';
    }
    trace_line("=== vk interface trace: pid=%d", (int)getpid());
    trace_line("cmd: %s", cmdline[0] ? cmdline : "(unknown)");
}

/* ------------------------------------------------------------------ */
/* query accounting                                                    */

static uint32_t name_hash(const char *s)
{
    uint32_t h = 2166136261u;
    while (*s) { h ^= (unsigned char)*s++; h *= 16777619u; }
    return h;
}

static void count_query(const char *name, int is_gdpa)
{
    if (!name || !*name) return;
    pthread_mutex_lock(&trace_lock);
    uint32_t i = name_hash(name) & (TRACER_SLOTS - 1);
    for (;;)
    {
        struct trace_entry *e = &trace_table[i];
        if (!e->name[0])
        {
            snprintf(e->name, sizeof(e->name), "%s", name);
            if (is_gdpa) e->gdpa = 1; else e->gipa = 1;
            break;
        }
        if (!strcmp(e->name, name))
        {
            if (is_gdpa) e->gdpa++; else e->gipa++;
            break;
        }
        i = (i + 1) & (TRACER_SLOTS - 1);
    }
    if (is_gdpa) total_gdpa++; else total_gipa++;
    pthread_mutex_unlock(&trace_lock);
}

static int entry_cmp(const void *a, const void *b)
{
    const struct trace_entry *x = a, *y = b;
    return (int)(y->gipa + y->gdpa) - (int)(x->gipa + x->gdpa);
}

static void trace_flush(int destroy)
{
    struct trace_entry sorted[TRACER_SLOTS];
    static const struct trace_entry empty;

    pthread_mutex_lock(&trace_lock);
    memcpy(sorted, trace_table, sizeof(sorted));
    pthread_mutex_unlock(&trace_lock);
    qsort(sorted, TRACER_SLOTS, sizeof(*sorted), entry_cmp);

    trace_line("--- query table: gipa_total=%u gdpa_total=%u missing_gipa=%u missing_gdpa=%u",
               total_gipa, total_gdpa, missing_gipa, missing_gdpa);
    unsigned unique = 0, batch_no = 0, batch_used = 0;
    char batch[400];
    for (unsigned i = 0; i < TRACER_SLOTS; i++)
    {
        struct trace_entry *e = &sorted[i];
        if (!memcmp(e, &empty, sizeof(empty))) break;
        trace_line("FN %-52s gipa=%-5u gdpa=%u", e->name, e->gipa, e->gdpa);
        unique++;
        if (!destroy)
        {
            if (batch_used == 0) batch[0] = 0;
            int n = snprintf(batch + batch_used, sizeof(batch) - batch_used, "%s%s(%u/%u)",
                             batch_used ? " " : "", e->name, e->gipa, e->gdpa);
            if (n > 0 && (size_t)n < sizeof(batch) - batch_used) batch_used += n;
            if (batch_used >= sizeof(batch) - 64 || !memcmp(&sorted[i + 1 < TRACER_SLOTS ? i + 1 : i],
                                                             &empty, sizeof(empty)))
            {
                LOG("TBL%03u %{public}s", batch_no++, batch);
                batch_used = 0;
            }
        }
    }
    trace_line("--- end of query table (%u unique)", unique);
    if (destroy)
        LOG("FLUSH end: gipa=%u gdpa=%u missing=%u/%u unique=%u",
            total_gipa, total_gdpa, missing_gipa, missing_gdpa, unique);
}

/* ------------------------------------------------------------------ */
/* helpers                                                             */

static const char *stype_name(uint32_t s)
{
    switch (s)
    {
    case 0: return "APPLICATION_INFO";
    case 1: return "DEVICE_QUEUE_CREATE_INFO";
    case 1000000001: return "PHYSICAL_DEVICE_FEATURES_2";
    case 1000142000: return "PHYSICAL_DEVICE_PUSH_DESCRIPTOR";
    case 1000063000: return "PHYSICAL_DEVICE_MULTIVIEW";
    case 1000166000: return "PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION";
    case 1000170000: return "PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS";
    case 1000189000: return "PHYSICAL_DEVICE_CONDITIONAL_RENDERING";
    case 1000206000: return "PHYSICAL_DEVICE_TRANSFORM_FEEDBACK";
    case 1000237000: return "PHYSICAL_DEVICE_SHADER_ATOMIC_INT64";
    case 1000257000: return "PHYSICAL_DEVICE_16BIT_STORAGE";
    case 1000280000: return "PHYSICAL_DEVICE_8BIT_STORAGE";
    case 1000299000: return "PHYSICAL_DEVICE_DRIVER_PROPERTIES";
    case 1000322000: return "PHYSICAL_DEVICE_TIMELINE_SEMAPHORE";
    case 1000342000: return "PHYSICAL_DEVICE_DESCRIPTOR_INDEXING";
    case 1000365000: return "PHYSICAL_DEVICE_DYNAMIC_RENDERING";
    case 1000378000: return "PHYSICAL_DEVICE_SYNCHRONIZATION_2";
    case 1000412000: return "PHYSICAL_DEVICE_MAINTENANCE_4";
    case 1000444000: return "PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE";
    case 1000453000: return "PHYSICAL_DEVICE_TEXEL_BUFFER_ALIGNMENT";
    case 1000475000: return "PHYSICAL_DEVICE_INLINE_UNIFORM_BLOCK";
    case 1000477000: return "PHYSICAL_DEVICE_4444_FORMATS";
    case 1000494000: return "PHYSICAL_DEVICE_WORKGROUP_MEMORY_EXPLICIT_LAYOUT";
    case 1000502000: return "PHYSICAL_DEVICE_PIPELINE_CREATION_CACHE_CONTROL";
    case 1000117000: return "PHYSICAL_DEVICE_SHADER_DRAW_PARAMETERS";
    case 1000148000: return "PHYSICAL_DEVICE_SHADER_FLOAT16_INT8";
    case 1000127000: return "PHYSICAL_DEVICE_VULKAN_1_1";
    case 1000177000: return "PHYSICAL_DEVICE_VULKAN_1_2";
    case 1000488000: return "PHYSICAL_DEVICE_VULKAN_1_3";
    default: return NULL;
    }
}

static void log_create_info_pnext(const char *tag, const void *pNext)
{
    /* Walk generic sType/pNext nodes. pNext sits at byte offset 8
     * (4-byte sType + alignment padding), NOT at sType size. */
    const char *p = pNext;
    int count = 0;
    while (p && count < 16)
    {
        uint32_t stype;
        const void *nextp;
        memcpy(&stype, p, sizeof(stype));
        memcpy(&nextp, p + 8, sizeof(nextp));
        const char *name = stype_name(stype);
        trace_line("PNEXT %s sType=%u%s%s", tag, stype, name ? " (" : "",
                   name ? name : "");
        p = nextp;
        count++;
    }
}

static void log_extensions(const char *tag, uint32_t count, const char *const *names)
{
    for (uint32_t i = 0; i < count && i < 256; i++)
        trace_line("EXT %s %s", tag, names[i]);
}

static void dump_supported_instance_extensions(void)
{
    PFN_vkEnumerateInstanceExtensionProperties enumerate =
        (PFN_vkEnumerateInstanceExtensionProperties)
        (next_gipa ? next_gipa(NULL, "vkEnumerateInstanceExtensionProperties") : NULL);
    if (!enumerate) return;
    uint32_t count = 0;
    if (enumerate(NULL, &count, NULL) != VK_SUCCESS || !count || count > 512) return;
    VkExtensionProperties *props = calloc(count, sizeof(*props));
    if (!props) return;
    if (enumerate(NULL, &count, props) == VK_SUCCESS)
        for (uint32_t i = 0; i < count; i++)
            trace_line("SUPP instance %s %u", props[i].extensionName, (unsigned)props[i].specVersion);
    free(props);
}

/* ------------------------------------------------------------------ */
/* intercepted entry points                                            */

VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(uint32_t *count, VkLayerProperties *props)
{
    if (!count) return VK_ERROR_INITIALIZATION_FAILED;
    if (!props) { *count = 1; return VK_SUCCESS; }
    if (!*count) return VK_INCOMPLETE;
    VkLayerProperties p = { .layerName = LAYER_NAME, .specVersion = VK_API_VERSION_1_0,
        .implementationVersion = 2, .description = "Codex vk interface tracer" };
    props[0] = p; *count = 1; return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *info,
        const VkAllocationCallbacks *allocator, VkInstance *instance)
{
    trace_header();
    LOG("ckpt1 enter");
    if (info->pApplicationInfo)
    {
        const VkApplicationInfo *app = info->pApplicationInfo;
        trace_line("APP api=%u.%u.%u appVer=%u engineVer=%u name=%s engine=%s",
                   VK_VERSION_MAJOR(app->apiVersion), VK_VERSION_MINOR(app->apiVersion),
                   VK_VERSION_PATCH(app->apiVersion), app->applicationVersion,
                   app->engineVersion, app->pApplicationName ? app->pApplicationName : "-",
                   app->pEngineName ? app->pEngineName : "-");
    }
    log_create_info_pnext("instance", info->pNext);
    log_extensions("instance-enabled", info->enabledExtensionCount, info->ppEnabledExtensionNames);
    for (uint32_t i = 0; i < info->enabledLayerCount && i < 32; i++)
        trace_line("LAYER enabled %s", info->ppEnabledLayerNames[i]);
    LOG("ckpt2 logged createinfo");

    VkLayerInstanceCreateInfo *link = (VkLayerInstanceCreateInfo *)info->pNext;
    while (link && (link->sType != VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO ||
                    link->function != VK_LAYER_LINK_INFO)) link = (void *)link->pNext;
    if (!link || !link->u.pLayerInfo) { LOG("missing loader link"); return VK_ERROR_INITIALIZATION_FAILED; }
    next_gipa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkCreateInstance next = (PFN_vkCreateInstance)next_gipa(NULL, "vkCreateInstance");
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    LOG("ckpt3 chain ready");
    trace_line("EVT vkCreateInstance enter");
    VkResult res = next(info, allocator, instance);
    LOG("ckpt4 chain returned res=%d", res);
    trace_line("EVT vkCreateInstance result=%d", res);
    if (res == VK_SUCCESS)
        dump_supported_instance_extensions();
    return res;
}

VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice physical_device,
        const VkDeviceCreateInfo *info, const VkAllocationCallbacks *allocator, VkDevice *device)
{
    log_create_info_pnext("device", info->pNext);
    log_extensions("device-enabled", info->enabledExtensionCount, info->ppEnabledExtensionNames);
    trace_line("EVT vkCreateDevice queues=%u", info->queueCreateInfoCount);

    VkLayerDeviceCreateInfo *link = (VkLayerDeviceCreateInfo *)info->pNext;
    while (link && (link->sType != VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO ||
                    link->function != VK_LAYER_LINK_INFO)) link = (void *)link->pNext;
    if (!link || !link->u.pLayerInfo) { LOG("missing device loader link"); return VK_ERROR_INITIALIZATION_FAILED; }
    next_gdpa = link->u.pLayerInfo->pfnNextGetDeviceProcAddr;
    PFN_vkGetInstanceProcAddr next_igpa = link->u.pLayerInfo->pfnNextGetInstanceProcAddr;
    PFN_vkCreateDevice next = (PFN_vkCreateDevice)next_igpa(NULL, "vkCreateDevice");
    link->u.pLayerInfo = link->u.pLayerInfo->pNext;
    VkResult res = next(physical_device, info, allocator, device);
    trace_line("EVT vkCreateDevice result=%d", res);
    if (res == VK_SUCCESS) trace_flush(0);
    return res;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char *name)
{
    if (!name) return NULL;
    if (instance) seen_instance = instance;
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)vkGetInstanceProcAddr;
    if (!strcmp(name, "vkEnumerateInstanceLayerProperties")) return (PFN_vkVoidFunction)vkEnumerateInstanceLayerProperties;
    if (!strcmp(name, "vkCreateInstance")) return (PFN_vkVoidFunction)vkCreateInstance;
    if (!next_gipa) return NULL;
    count_query(name, 0);
    PFN_vkVoidFunction fn = next_gipa(instance, name);
    if (!fn)
    {
        trace_line("MISSING gipa %s", name);
        pthread_mutex_lock(&trace_lock); missing_gipa++; pthread_mutex_unlock(&trace_lock);
    }
    return fn;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char *name)
{
    if (!name) return NULL;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)vkGetDeviceProcAddr;
    if (!next_gdpa) return NULL;
    count_query(name, 1);
    PFN_vkVoidFunction fn = next_gdpa(device, name);
    if (!fn)
    {
        trace_line("MISSING gdpa %s", name);
        pthread_mutex_lock(&trace_lock); missing_gdpa++; pthread_mutex_unlock(&trace_lock);
    }
    return fn;
}

__attribute__((destructor)) static void tracer_unload(void)
{
    trace_flush(1);
    pthread_mutex_lock(&trace_lock);
    if (tracef) fclose(tracef);
    tracef = NULL;
    pthread_mutex_unlock(&trace_lock);
}
