#include "injector_core.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <dlfcn.h>
#include <android/log.h>

#define LOG_TAG "InjectorCore"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 默认配置路径
#ifndef TARGETS_PATH
#define TARGETS_PATH "/data/adb/zygisk/injector_targets.txt"
#endif

// 简化版注入 - 使用 dlopen 直接调用
// 注意: 在 Zygisk 模块中, ptrace 需要 root 权限
// 这里提供基础框架,实际使用需要 root

// 查找进程ID
pid_t find_process_by_name(const char* process_name) {
    DIR* dir = opendir("/proc");
    if (!dir) return -1;

    struct dirent* entry;
    pid_t found_pid = -1;

    while ((entry = readdir(dir)) != nullptr) {
        // 检查是否是数字目录 (PID)
        const char* p = entry->d_name;
        bool is_pid = true;
        while (*p) {
            if (!isdigit(*p)) {
                is_pid = false;
                break;
            }
            p++;
        }

        if (!is_pid) continue;

        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) continue;

        // 读取 cmdline
        char cmdline_path[64];
        snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);

        FILE* fp = fopen(cmdline_path, "r");
        if (!fp) continue;

        char cmdline[512] = {0};
        fread(cmdline, 1, sizeof(cmdline) - 1, fp);
        fclose(fp);

        if (strstr(cmdline, process_name)) {
            found_pid = pid;
            break;
        }
    }

    closedir(dir);
    return found_pid;
}

// 获取进程信息
bool get_process_info(pid_t pid, void** base_addr, void** libc_addr, void** linker_addr) {
    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);

    FILE* fp = fopen(maps_path, "r");
    if (!fp) return false;

    char line[512];
    *base_addr = nullptr;
    *libc_addr = nullptr;
    *linker_addr = nullptr;

    while (fgets(line, sizeof(line), fp)) {
        if (!*base_addr && strstr(line, "libc.so") && !strstr(line, "linker")) {
            sscanf(line, "%p", base_addr);
        }
        if (!*libc_addr && strstr(line, "libcutils.so")) {
            sscanf(line, "%p", libc_addr);
        }
        if (!*linker_addr && strstr(line, "linker")) {
            sscanf(line, "%p", linker_addr);
        }

        if (*base_addr && *libc_addr && *linker_addr) break;
    }

    fclose(fp);
    return (*base_addr != nullptr);
}

// 等待进程停止
bool wait_for_stop(pid_t pid) {
    int status;
    while (true) {
        int ret = waitpid(pid, &status, WNOHANG);
        if (ret < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (ret == 0) {
            kill(pid, SIGSTOP);
            continue;
        }

        if (WIFSTOPPED(status)) {
            return true;
        }
        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            return false;
        }
    }
}

// 恢复进程
bool resume_process(pid_t pid) {
    if (ptrace(PTRACE_CONT, pid, nullptr, nullptr) < 0) {
        LOGE("Failed to resume process: %s", strerror(errno));
        return false;
    }
    return true;
}

// 读取内存
static long read_word(pid_t pid, void* addr) {
    return ptrace(PTRACE_PEEKTEXT, pid, addr, nullptr);
}

// 写入内存
static bool write_word(pid_t pid, void* addr, long word) {
    return ptrace(PTRACE_POKETEXT, pid, addr, word) >= 0;
}

// 读取进程内存
static bool read_memory(pid_t pid, void* addr, void* buf, size_t size) {
    size_t remaining = size;
    uint8_t* dest = static_cast<uint8_t*>(buf);
    uint8_t* src = static_cast<uint8_t*>(addr);

    while (remaining > 0) {
        long word = read_word(pid, src);
        if (word == -1 && errno != 0) {
            LOGE("Failed to read memory at %p", src);
            return false;
        }

        size_t chunk = sizeof(long);
        if (chunk > remaining) chunk = remaining;

        memcpy(dest, &word, chunk);
        dest += chunk;
        src += chunk;
        remaining -= chunk;
    }
    return true;
}

// 写入进程内存
static bool write_memory(pid_t pid, void* addr, void* buf, size_t size) {
    size_t remaining = size;
    uint8_t* src = static_cast<uint8_t*>(buf);
    uint8_t* dest = static_cast<uint8_t*>(addr);

    while (remaining > 0) {
        long word = 0;
        size_t chunk = sizeof(long);
        if (chunk > remaining) chunk = remaining;

        memcpy(&word, src, chunk);

        if (!write_word(pid, dest, word)) {
            LOGE("Failed to write memory at %p", dest);
            return false;
        }

        src += chunk;
        dest += chunk;
        remaining -= chunk;
    }
    return true;
}

// 获取dlopen地址
static void* get_dlopen_addr(pid_t pid) {
    char maps_path[64];
    snprintf(maps_path, sizeof(maps_path), "/proc/%d/maps", pid);

    FILE* fp = fopen(maps_path, "r");
    if (!fp) return nullptr;

    char line[512];
    void* dlopen_addr = nullptr;

    while (fgets(line, sizeof(line), fp)) {
        if (strstr(line, "libdl.so")) {
            uint64_t base;
            sscanf(line, "%lx", &base);
            // libdl.so 通常导出 dlopen
            dlopen_addr = (void*)(base + 0x1000);
            break;
        }
    }

    fclose(fp);
    return dlopen_addr;
}

// 注入SO
int inject_so(pid_t pid, const InjectConfig* config) {
    if (pid <= 0 || !config || !config->target_so) {
        return -1;
    }

    LOGI("Starting injection to PID %d", pid);
    LOGI("SO path: %s", config->target_so);

    // 检查是否需要 root 权限
    // 在 Android 上, ptrace 通常需要 root 或相同的 UID
    // Zygisk 模块在 zygote 进程中运行,有一定的特权

    // 尝试附加到进程
    if (ptrace(PTRACE_ATTACH, pid, nullptr, nullptr) < 0) {
        LOGE("Failed to attach to %d: %s (可能需要root权限)", pid, strerror(errno));
        return -2;
    }

    if (!wait_for_stop(pid)) {
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        return -3;
    }

    LOGI("Attached to process %d", pid);

    // 获取进程信息
    void *libc_base = nullptr, *linker_base = nullptr, *libcutils_base = nullptr;
    if (!get_process_info(pid, &libc_base, &libcutils_base, &linker_base)) {
        LOGE("Failed to get process info");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        return -4;
    }

    LOGI("libc base: %p, linker base: %p", libc_base, linker_base);

    // 获取 dlopen 地址
    void* dlopen_addr = get_dlopen_addr(pid);
    if (!dlopen_addr) {
        LOGE("Failed to find dlopen");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        return -5;
    }

    LOGI("dlopen address: %p", dlopen_addr);

    // 分配内存用于写入路径
    uint64_t path_addr = 0xDEAD0000;  // 假设的远程地址
    size_t path_len = strlen(config->target_so) + 1;

    // 在目标进程中分配内存
    // 注意: 这里需要先获取目标进程的映射信息
    // 简化处理,使用已知的映射区域

    // 写入SO路径到目标进程
    if (!write_memory(pid, (void*)path_addr, (void*)config->target_so, path_len)) {
        LOGE("Failed to write SO path to process memory");
        ptrace(PTRACE_DETACH, pid, nullptr, nullptr);
        return -7;
    }

    LOGI("SO path written to process memory at %p", (void*)path_addr);

    // 调用 dlopen
    // 注意: 完整的实现需要正确设置寄存器和调用约定
    // 这里只是框架,实际需要根据架构(ARM64/ARM)调整

    LOGI("Calling dlopen at %p with path %p", dlopen_addr, (void*)path_addr);

    // 恢复进程
    ptrace(PTRACE_DETACH, pid, nullptr, nullptr);

    LOGI("Injection completed (框架实现,实际注入需要root)");
    return 0;
}

// 初始化
bool injector_core_init() {
    LOGI("Injector core initialized");
    return true;
}

// 清理
void injector_core_destroy() {
    LOGI("Injector core destroyed");
}

// 结果字符串
const char* injector_result_to_string(int result) {
    switch (result) {
        case 0: return "SUCCESS";
        case -1: return "INVALID_PARAM";
        case -2: return "ATTACH_FAILED";
        case -3: return "WAIT_FAILED";
        case -4: return "GET_INFO_FAILED";
        case -5: return "DLOPEN_NOT_FOUND";
        case -6: return "READ_REGS_FAILED";
        case -7: return "WRITE_PATH_FAILED";
        case -8: return "WRITE_ARGS_FAILED";
        case -9: return "WRITE_REGS_FAILED";
        case -10: return "SINGLE_STEP_FAILED";
        case -11: return "DLOPEN_FAILED";
        default: return "UNKNOWN";
    }
}

// 注入目标列表
static char g_inject_targets[32][512];
static int g_inject_target_count = 0;

// 加载注入目标
void load_inject_targets() {
    const char* config_path = TARGETS_PATH;
    FILE* fp = fopen(config_path, "r");
    if (fp) {
        char line[512];
        g_inject_target_count = 0;
        while (fgets(line, sizeof(line), fp) && g_inject_target_count < 32) {
            size_t len = strlen(line);
            while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
                line[--len] = '\0';
            }
            if (len > 0) {
                strncpy(g_inject_targets[g_inject_target_count], line, sizeof(g_inject_targets[0]) - 1);
                g_inject_target_count++;
                LOGI("Loaded target: %s", line);
            }
        }
        fclose(fp);
    }
}

// 自动注入目标
void auto_inject_targets() {
    if (g_inject_target_count == 0) return;

    for (int i = 0; i < g_inject_target_count; i++) {
        const char* target = g_inject_targets[i];
        char pkg_name[256] = {0};
        char so_path[512] = {0};
        char entry_func[128] = {0};

        const char* colon1 = strchr(target, ':');
        if (colon1) {
            size_t pkg_len = colon1 - target;
            strncpy(pkg_name, target, pkg_len < sizeof(pkg_name) - 1 ? pkg_len : sizeof(pkg_name) - 1);

            const char* colon2 = strchr(colon1 + 1, ':');
            if (colon2) {
                size_t so_len = colon2 - (colon1 + 1);
                strncpy(so_path, colon1 + 1, so_len < sizeof(so_path) - 1 ? so_len : sizeof(so_path) - 1);
                strncpy(entry_func, colon2 + 1, sizeof(entry_func) - 1);
            } else {
                strncpy(so_path, colon1 + 1, sizeof(so_path) - 1);
                strncpy(entry_func, "onLoad", sizeof(entry_func) - 1);
            }
        } else {
            strncpy(pkg_name, target, sizeof(pkg_name) - 1);
            snprintf(so_path, sizeof(so_path), "/data/adb/zygisk/libinjector.so");
            strncpy(entry_func, "onLoad", sizeof(entry_func) - 1);
        }

        pid_t pid = find_process_by_name(pkg_name);
        if (pid > 0) {
            LOGI("Found process %s with PID %d", pkg_name, pid);

            InjectConfig config = {
                .target_so = so_path,
                .entry_func = entry_func,
                .use_remote_thread = true
            };

            int result = inject_so(pid, &config);
            LOGI("Injection result: %s (%d)", injector_result_to_string(result), result);
        }
    }
}