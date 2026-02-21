#include <android/log.h>
#include <cstring>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <mutex>
#include <thread>
#include <vector>
#include <string>

#include "zygisk_next_api.h"
#include "libinjector.h"
#include "injector_core.h"

#define LOG_TAG "ZygiskInjector"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)

static ZygiskNextAPI* g_api = nullptr;
static void* g_handle = nullptr;
static bool g_initialized = false;
static std::mutex g_inject_mutex;

// 注入状态
static bool g_is_injected = false;
static char g_injected_pkg[256] = {0};
static char g_injected_so[512] = {0};

// 配置路径
static const char* CONFIG_PATH = "/data/adb/zygisk/injector_config.txt";
static const char* TARGETS_PATH = "/data/adb/zygisk/injector_targets.txt";

// 解析配置行
static void parse_config_line(const char* line, char* pkg, char* so_path, char* entry_func) {
    pkg[0] = '\0';
    so_path[0] = '\0';
    entry_func[0] = '\0';

    const char* colon1 = strchr(line, ':');
    if (colon1) {
        size_t pkg_len = colon1 - line;
        strncpy(pkg, line, pkg_len < 255 ? pkg_len : 255);

        const char* colon2 = strchr(colon1 + 1, ':');
        if (colon2) {
            size_t so_len = colon2 - (colon1 + 1);
            strncpy(so_path, colon1 + 1, so_len < 511 ? so_len : 511);
            strncpy(entry_func, colon2 + 1, 127);
        } else {
            strncpy(so_path, colon1 + 1, 511);
            strncpy(entry_func, "onLoad", 127);
        }
    }
}

// 加载注入配置
static void load_inject_config() {
    FILE* fp = fopen(CONFIG_PATH, "r");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) {
            // 去除换行符
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                line[--len] = '\0';
            }
            if (len > 0 && line[0] != '#') {
                char pkg[256], so_path[512], entry_func[128];
                parse_config_line(line, pkg, so_path, entry_func);
                LOGI("Config: pkg=%s, so=%s, func=%s", pkg, so_path, entry_func);
            }
        }
        fclose(fp);
    }
}

// 查找进程
static pid_t find_process(const char* pkg_name) {
    char cmdline_path[64];
    snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", getpid());

    FILE* fp = fopen(cmdline_path, "r");
    if (!fp) return -1;

    char cmdline[512] = {0};
    fread(cmdline, 1, sizeof(cmdline) - 1, fp);
    fclose(fp);

    return strstr(cmdline, pkg_name) ? getpid() : -1;
}

// 执行注入
static int perform_inject(const char* pkg, const char* so_path, const char* entry_func) {
    std::lock_guard<std::mutex> lock(g_inject_mutex);

    if (g_is_injected && strcmp(g_injected_pkg, pkg) == 0) {
        LOGI("Already injected: %s", pkg);
        return 0;
    }

    pid_t pid = find_process(pkg);
    if (pid < 0) {
        LOGE("Process not found: %s", pkg);
        return -1;
    }

    InjectConfig config = {
        .target_so = so_path,
        .entry_func = entry_func,
        .use_remote_thread = true
    };

    int result = inject_so(pid, &config);

    if (result == 0) {
        g_is_injected = true;
        strncpy(g_injected_pkg, pkg, sizeof(g_injected_pkg) - 1);
        strncpy(g_injected_so, so_path, sizeof(g_injected_so) - 1);
    }

    return result;
}

// 回调: 收到注入请求
static bool on_inject_request(InjectRequest* req) {
    LOGI("Inject request: pkg=%s, so=%s, func=%s",
         req->target_pkg, req->so_path, req->entry_func);

    int result = perform_inject(req->target_pkg, req->so_path, req->entry_func);

    // 发送结果
    if (g_api && g_handle) {
        int fd = g_api->connectCompanion(g_handle);
        if (fd >= 0) {
            InjectResultMsg msg = {result, ""};
            strncpy(msg.message, injector_result_to_string(result), sizeof(msg.message) - 1);
            write(fd, &msg, sizeof(msg));
            close(fd);
        }
    }

    return result == 0;
}

// 回调: 收到卸载请求
static bool on_unload_request() {
    LOGI("Unload request");
    g_is_injected = false;
    g_injected_pkg[0] = '\0';
    g_injected_so[0] = '\0';
    return true;
}

// 回调: 发送消息到App
static int send_to_app(uint32_t type, void* data, uint32_t size) {
    if (!g_api || !g_handle) return -1;

    int fd = g_api->connectCompanion(g_handle);
    if (fd < 0) return -1;

    InjectorMessage msg;
    msg.type = type;
    msg.size = size;
    memcpy(msg.data, data, size);

    int ret = write(fd, &msg, sizeof(msg));
    close(fd);
    return ret;
}

// 回调: 收到Hook配置
static bool on_hook_config(HookConfig* config) {
    LOGI("Hook config: lib=%s, sym=%s", config->target_lib, config->target_sym);
    // TODO: 实现Hook功能
    return true;
}

// Companion: 连接建立
static void on_module_connected(int fd) {
    LOGI("App connected");

    InjectorMessage msg;
    ssize_t ret = read(fd, &msg, sizeof(msg));

    if (ret > 0) {
        LOGI("Received message: type=%u, size=%u", msg.type, msg.size);

        switch (msg.type) {
            case MSG_INJECT: {
                InjectRequest* req = reinterpret_cast<InjectRequest*>(msg.data);
                bool success = on_inject_request(req);
                LOGI("Inject result: %s", success ? "success" : "failed");
                break;
            }
            case MSG_UNLOAD:
                on_unload_request();
                break;
            case MSG_GET_STATUS: {
                StatusResponse status = {
                    .is_injected = g_is_injected,
                    .hook_active = false,
                    .api_version = 1
                };
                strncpy(status.injected_pkg, g_injected_pkg, sizeof(status.injected_pkg) - 1);
                InjectorMessage resp;
                resp.type = MSG_RESPONSE;
                resp.size = sizeof(status);
                memcpy(resp.data, &status, sizeof(status));
                write(fd, &resp, sizeof(resp));
                break;
            }
        }
    }

    close(fd);
}

// Companion: 加载完成
static void on_companion_loaded() {
    LOGI("Companion loaded");

    // 初始化注入器核心
    injector_core_init();

    // 加载配置
    load_inject_config();
}

// 模块加载完成
void onModuleLoaded(void* self_handle, const ZygiskNextAPI* api) {
    LOGI("Module loaded");

    if (!api) {
        LOGE("API is null");
        return;
    }

    g_api = const_cast<ZygiskNextAPI*>(api);
    g_handle = self_handle;

    // 初始化通信
    InjectorCallbacks callbacks = {
        .onInjectRequest = on_inject_request,
        .onHookConfig = on_hook_config,
        .onUnloadRequest = on_unload_request,
        .sendToApp = send_to_app
    };

    if (!injector_init(&callbacks)) {
        LOGE("Failed to init injector communication");
        return;
    }

    g_initialized = true;

    // 加载注入目标
    load_inject_targets();

    LOGI("Zygisk Injector initialized successfully");
}

// Companion 模块定义
__attribute__((visibility("default"), unused))
struct ZygiskNextCompanionModule zn_companion_module = {
    .target_api_version = ZYGISK_NEXT_API_VERSION_1,
    .onCompanionLoaded = on_companion_loaded,
    .onModuleConnected = on_module_connected
};
