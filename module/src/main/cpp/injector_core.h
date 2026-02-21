#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

// 注入配置
typedef struct {
    const char* target_so;      // 要注入的SO路径
    const char* entry_func;     // 入口函数名
    bool use_remote_thread;     // 使用远程线程调用
} InjectConfig;

// 初始化注入器
bool injector_core_init();

// 清理注入器
void injector_core_destroy();

// 获取注入结果字符串
const char* injector_result_to_string(int result);

// 执行SO注入
int inject_so(pid_t pid, const InjectConfig* config);

// 查找进程ID
pid_t find_process_by_name(const char* process_name);

// 获取进程信息
bool get_process_info(pid_t pid, void** base_addr, void** libc_addr, void** linker_addr);

// 等待进程停止
bool wait_for_stop(pid_t pid);

// 恢复进程
bool resume_process(pid_t pid);

// 加载注入目标
void load_inject_targets();

// 自动注入目标
void auto_inject_targets();

#ifdef __cplusplus
}
#endif