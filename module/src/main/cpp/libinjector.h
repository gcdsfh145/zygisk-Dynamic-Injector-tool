#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

// 注入器与模块之间的通信协议

#define INJECTOR_SOCKET_NAME "libinjector_socket"
#define INJECTOR_MAX_MSG_SIZE 4096

// 消息类型
typedef enum {
    MSG_PING = 0,           // 心跳检测
    MSG_INJECT,             // 注入请求
    MSG_INJECT_RESULT,      // 注入结果
    MSG_HOOK_CONFIG,        // Hook配置
    MSG_HOOK_RESULT,        // Hook结果
    MSG_UNLOAD,             // 卸载模块
    MSG_GET_STATUS,         // 获取状态
    MSG_RESPONSE = 100      // 响应消息
} InjectorMessageType;

// 注入结果
typedef enum {
    INJECT_SUCCESS = 0,
    INJECT_ALREADY_INJECTED,
    INJECT_FAILED,
    INJECT_PERMISSION_DENIED
} InjectResult;

// 通用消息头
struct InjectorMessage {
    uint32_t type;          // 消息类型
    uint32_t size;          // 数据大小
    uint8_t data[];         // 消息数据
};

// 注入请求消息
struct InjectRequest {
    char target_pkg[256];   // 目标包名
    char so_path[512];      // 要注入的SO路径
    char entry_func[128];   // 入口函数名
};

// Hook配置消息
struct HookConfig {
    char target_lib[256];   // 目标库
    char target_sym[256];   // 目标符号
    uint64_t hook_addr;     // Hook地址
    uint64_t replace_addr;  // 替换地址
};

// 状态响应
struct StatusResponse {
    bool is_injected;       // 是否已注入
    bool hook_active;       // Hook是否激活
    char injected_pkg[256]; // 注入的包名
    uint32_t api_version;   // API版本
};

// 注入结果消息
struct InjectResultMsg {
    int result;
    char message[256];
};

// 通信回调
struct InjectorCallbacks {
    // 收到注入请求
    bool (*onInjectRequest)(struct InjectRequest* req);

    // 收到Hook配置
    bool (*onHookConfig)(struct HookConfig* config);

    // 收到卸载请求
    bool (*onUnloadRequest)();

    // 发送消息到App
    int (*sendToApp)(uint32_t type, void* data, uint32_t size);
};

// 初始化通信
bool injector_init(struct InjectorCallbacks* callbacks);

// 清理通信
void injector_destroy();

// 发送消息到App
int injector_send_to_app(uint32_t type, void* data, uint32_t size);

// 检查通信状态
bool injector_is_connected();

// 获取状态
void injector_get_status(struct StatusResponse* status);

#ifdef __cplusplus
}
#endif