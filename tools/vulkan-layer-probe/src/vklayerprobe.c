/*
 * vklayerprobe - diagnose OHOS Vulkan loader layer discovery.
 *
 * On HarmonyOS the system libvulkan.so reads all loader configuration
 * (VK_LAYER_PATH, HOME, XDG_*, ...) through OHOS system parameters
 * (CachedParameterCreate), never through process environment variables.
 * App-provided layers are discovered via the debug parameter pair:
 *
 *   param set debug.graphic.debug_layer <manifest basename>
 *   param set debug.graphic.debug_hap   <this app's hap/bundle name>
 *
 * The loader then reads the manifest from
 *   /data/storage/el2/base/haps/entry/files/<debug_layer>.json
 * (path hardcoded in libvulkan.so; InitBundleInfo gates it to the app
 * whose GetBundleInfoForSelf name matches debug.graphic.debug_hap).
 *
 * This probe writes an implicit-layer manifest there (implicit so that
 * Vulkan instances created without enabledLayerNames - e.g. DXVK under
 * Wine - are intercepted too) and verifies discovery in-process.
 */
#define _GNU_SOURCE
#include <dirent.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <hilog/log.h>
#include <napi/native_api.h>
#include <vulkan/vulkan.h>

#define LOG(fmt, ...) OH_LOG_Print(LOG_APP, LOG_INFO, 0x0000, "VkLayerProbe", fmt, ##__VA_ARGS__)

#define LAYER_NAME     "VK_LAYER_CODEX_probe"
#define MANIFEST_BASE  "codex_layer_probe"
#define MANIFEST_DIR   "/data/storage/el2/base/haps/entry/files"
#define MANIFEST_PATH  MANIFEST_DIR "/" MANIFEST_BASE ".json"
#define TRACE_DIR      MANIFEST_DIR "/vk-trace"

/* Mirror a completed on-device trace file (written by the layer inside
 * wine child processes) through hilog, then mark it as dumped by
 * renaming. Recovery path for lines lost to hilog flow control. */
static void dump_trace_files(void)
{
    DIR *d = opendir(TRACE_DIR);
    if (!d) return;
    struct dirent *de;
    while ((de = readdir(d)))
    {
        const char *n = de->d_name;
        size_t len = strlen(n);
        if (len < 5 || strcmp(n + len - 4, ".log") || strstr(n, ".dumped"))
            continue;
        char path[256], path2[256];
        snprintf(path, sizeof(path), TRACE_DIR "/%s", n);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        LOG("DUMP begin %{public}s", n);
        char line[480];
        unsigned count = 0;
        while (fgets(line, sizeof(line), f))
        {
            size_t l = strlen(line);
            while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
            LOG("TRC %{public}s", line);
            count++;
        }
        fclose(f);
        LOG("DUMP end %{public}s lines=%{public}u", n, count);
        snprintf(path2, sizeof(path2), "%s.dumped", path);
        rename(path, path2);
    }
    closedir(d);
}

static char layer_lib_path[2048];

static void ensure_dir(const char *path)
{
    if (mkdir(path, 0700) == 0 || errno == EEXIST) return;
    char tmp[256];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++)
    {
        if (*p == '/')
        {
            *p = 0;
            mkdir(tmp, 0700);
            *p = '/';
        }
    }
    mkdir(tmp, 0700);
}

static int write_manifest(void)
{
    ensure_dir(MANIFEST_DIR);
    FILE *f = fopen(MANIFEST_PATH, "w");
    if (!f)
    {
        LOG("manifest open failed errno=%{public}d path=%{public}s", errno, MANIFEST_PATH);
        return -1;
    }
    fprintf(f,
        "{\n"
        "  \"file_format_version\": \"1.1.2\",\n"
        "  \"layer\": {\n"
        "    \"name\": \"%s\",\n"
        "    \"type\": \"GLOBAL\",\n"
        "    \"library_path\": \"%s\",\n"
        "    \"api_version\": \"1.3.0\",\n"
        "    \"implementation_version\": \"1\",\n"
        "    \"description\": \"Codex Vulkan probe\",\n"
        "    \"disable_environment\": { \"DISABLE_CODEX_VK_PROBE\": \"1\" }\n"
        "  }\n"
        "}\n", LAYER_NAME, layer_lib_path);
    fclose(f);
    LOG("manifest written lib=%{public}s", layer_lib_path);
    return 0;
}

static void locate_layer_library(void)
{
    Dl_info dli;
    if (dladdr((void *)locate_layer_library, &dli) && dli.dli_fname)
    {
        snprintf(layer_lib_path, sizeof(layer_lib_path), "%s", dli.dli_fname);
        char *slash = strrchr(layer_lib_path, '/');
        if (slash) slash[1] = 0;
        strncat(layer_lib_path, "libVkLayer_codex_probe.so",
                sizeof(layer_lib_path) - strlen(layer_lib_path) - 1);
        return;
    }
    snprintf(layer_lib_path, sizeof(layer_lib_path), "libVkLayer_codex_probe.so");
}

static void enumerate_layers(void)
{
    uint32_t count = 0, i;
    VkResult result = vkEnumerateInstanceLayerProperties(&count, NULL);
    LOG("enumerate result=%{public}d count=%{public}u", result, count);
    if (result != VK_SUCCESS || !count || count > 64) return;
    VkLayerProperties *layers = calloc(count, sizeof(*layers));
    if (!layers) return;
    result = vkEnumerateInstanceLayerProperties(&count, layers);
    LOG("enumerate details result=%{public}d count=%{public}u", result, count);
    for (i = 0; i < count; i++)
        LOG("layer[%{public}u]=%{public}s", i, layers[i].layerName);
    free(layers);
}

static void create_instance(const char *tag, const char *const *names, uint32_t n)
{
    VkInstanceCreateInfo ci = { .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                .enabledLayerCount = n, .ppEnabledLayerNames = names };
    VkInstance inst = VK_NULL_HANDLE;
    VkResult r = vkCreateInstance(&ci, NULL, &inst);
    LOG("%{public}s create=%{public}d instance=%{public}p", tag, r, (void *)inst);
    if (r == VK_SUCCESS)
    {
        /* If the implicit layer is active its constructor already logged
         * "layer library loaded" and vkCreateInstance logged
         * "intercepted vkCreateInstance" with tag CodexVkLayer. */
        vkDestroyInstance(inst, NULL);
    }
}

static napi_value probe(napi_env env, napi_callback_info info)
{
    locate_layer_library();
    LOG("layer lib=%{public}s accessible=%{public}d", layer_lib_path,
        access(layer_lib_path, R_OK) == 0);

    if (write_manifest() != 0)
    {
        napi_value value;
        napi_get_undefined(env, &value);
        return value;
    }

    enumerate_layers();

    /* Implicit check: no enabledLayerNames; the layer must still be chained
     * in by the loader if the debug-layer mechanism works. */
    create_instance("implicit", NULL, 0);

    /* Explicit check as fallback evidence that the manifest itself is valid. */
    const char *name = LAYER_NAME;
    create_instance("explicit", &name, 1);

    /* Mirror any completed trace files from previous wine processes. */
    dump_trace_files();

    /* Manifest is intentionally left in place: the Wine game process
     * (same bundle, same debug.graphic.* parameters) needs it to get its
     * Vulkan calls intercepted by the layer. */
    napi_value value;
    napi_get_undefined(env, &value);
    return value;
}

static napi_value init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = { { .utf8name = "run", .method = probe } };
    napi_define_properties(env, exports, 1, desc);
    return exports;
}

static napi_module module = { .nm_version = 1, .nm_register_func = init, .nm_modname = "vklayerprobe" };
__attribute__((constructor)) static void register_module(void) { napi_module_register(&module); }
