#include "libinjector.h"
#include "zygisk_next_api.h"
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <android/log.h>
#include <mutex>
#include <thread>

#define LOG_TAG "libinjector"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

static struct InjectorCallbacks g_callbacks;
static bool g_initialized = false;
static bool g_connected = false;
static int g_socket_fd = -1;
static std::mutex g_socket_mutex;
static std::thread g_recv_thread;
static char g_socket_path[512];

// 接收消息线程
static void recv_thread_func() {
    while (g_connected && g_socket_fd >= 0) {
        struct InjectorMessage msg;
        ssize_t ret = recv(g_socket_fd, &msg, sizeof(msg), MSG_PEEK);

        if (ret < 0) {
            LOGE("recv error: %zd", ret);
            break;
        }

        if (ret < (ssize_t)sizeof(msg)) {
            continue;
        }

        if (msg.size > INJECTOR_MAX_MSG_SIZE) {
            LOGE("message too large: %u", msg.size);
            break;
        }

        uint32_t total_size = sizeof(msg) + msg.size;
        auto* full_msg = (struct InjectorMessage*)malloc(total_size);

        ret = recv(g_socket_fd, full_msg, total_size, 0);
        if (ret != (ssize_t)total_size) {
            LOGE("recv incomplete: %zd/%u", ret, total_size);
            free(full_msg);
            break;
        }

        // 处理消息
        switch (full_msg->type) {
            case MSG_INJECT: {
                if (full_msg->size == sizeof(struct InjectRequest) && g_callbacks.onInjectRequest) {
                    auto* req = (struct InjectRequest*)full_msg->data;
                    bool success = g_callbacks.onInjectRequest(req);

                    // 发送响应
                    uint32_t resp = success ? INJECT_SUCCESS : INJECT_FAILED;
                    injector_send_to_app(MSG_INJECT_RESULT, &resp, sizeof(resp));
                }
                break;
            }

            case MSG_HOOK_CONFIG: {
                if (full_msg->size == sizeof(struct HookConfig) && g_callbacks.onHookConfig) {
                    auto* config = (struct HookConfig*)full_msg->data;
                    bool success = g_callbacks.onHookConfig(config);

                    uint32_t resp = success ? 0 : 1;
                    injector_send_to_app(MSG_HOOK_RESULT, &resp, sizeof(resp));
                }
                break;
            }

            case MSG_UNLOAD: {
                if (g_callbacks.onUnloadRequest) {
                    g_callbacks.onUnloadRequest();
                }
                injector_send_to_app(MSG_RESPONSE, nullptr, 0);
                break;
            }

            case MSG_GET_STATUS: {
                struct StatusResponse status;
                injector_get_status(&status);
                injector_send_to_app(MSG_RESPONSE, &status, sizeof(status));
                break;
            }

            default:
                LOGE("unknown msg type: %u", full_msg->type);
                break;
        }

        free(full_msg);
    }

    g_connected = false;
    LOGI("recv thread exit");
}

bool injector_init(struct InjectorCallbacks* callbacks) {
    if (!callbacks || !callbacks->sendToApp) {
        LOGE("invalid callbacks");
        return false;
    }

    memcpy(&g_callbacks, callbacks, sizeof(g_callbacks));

    // 获取socket路径 (从环境变量或已知路径)
    const char* data_dir = getenv("INJECTOR_SOCKET_DIR");
    if (!data_dir) {
        data_dir = "/data/local/tmp";
    }

    snprintf(g_socket_path, sizeof(g_socket_path), "%s/%s.sock",
             data_dir, INJECTOR_SOCKET_NAME);

    // 创建Unix Domain Socket
    g_socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_socket_fd < 0) {
        LOGE("create socket failed");
        return false;
    }

    // 设置非阻塞
    int flags = fcntl(g_socket_fd, F_GETFL, 0);
    fcntl(g_socket_fd, F_SETFL, flags | O_NONBLOCK);

    // 绑定到路径
    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_socket_path, sizeof(addr.sun_path) - 1);

    unlink(g_socket_path); // 清理旧的socket

    if (bind(g_socket_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        LOGE("bind socket failed: %s", g_socket_path);
        close(g_socket_fd);
        g_socket_fd = -1;
        return false;
    }

    // 设置权限 (让Zygisk Companion可以访问)
    fchmod(g_socket_fd, 0777);

    // 开始监听
    if (listen(g_socket_fd, 5) < 0) {
        LOGE("listen failed");
        close(g_socket_fd);
        g_socket_fd = -1;
        return false;
    }

    g_initialized = true;
    LOGI("injector initialized: %s", g_socket_path);

    return true;
}

void injector_destroy() {
    g_connected = false;

    if (g_recv_thread.joinable()) {
        g_recv_thread.join();
    }

    if (g_socket_fd >= 0) {
        close(g_socket_fd);
        g_socket_fd = -1;
    }

    unlink(g_socket_path);

    g_initialized = false;
    LOGI("injector destroyed");
}

int injector_send_to_app(uint32_t type, void* data, uint32_t size) {
    if (!g_connected || g_socket_fd < 0) {
        return -1;
    }

    uint32_t total_size = sizeof(struct InjectorMessage) + size;
    auto* msg = (struct InjectorMessage*)malloc(total_size);

    msg->type = type;
    msg->size = size;
    if (data && size > 0) {
        memcpy(msg->data, data, size);
    }

    std::lock_guard<std::mutex> lock(g_socket_mutex);
    ssize_t ret = send(g_socket_fd, msg, total_size, 0);

    free(msg);

    if (ret < 0) {
        LOGE("send failed");
        return -1;
    }

    return ret;
}

bool injector_is_connected() {
    return g_connected;
}

// 供Companion调用的连接函数
bool injector_connect_to_app() {
    if (!g_initialized || g_socket_fd < 0) {
        return false;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, g_socket_path, sizeof(addr.sun_path) - 1);

    int client_fd = connect(g_socket_fd, (struct sockaddr*)&addr, sizeof(addr));
    if (client_fd < 0) {
        LOGE("connect to app failed");
        return false;
    }

    // 接收连接
    int conn_fd = accept(g_socket_fd, nullptr, nullptr);
    if (conn_fd < 0) {
        LOGE("accept failed");
        return false;
    }

    // 关闭原socket，使用连接socket
    close(g_socket_fd);
    g_socket_fd = conn_fd;
    g_connected = true;

    // 启动接收线程
    g_recv_thread = std::thread(recv_thread_func);

    LOGI("connected to app");
    return true;
}

void injector_get_status(struct StatusResponse* status) {
    if (!status) return;

    memset(status, 0, sizeof(*status));
    status->is_injected = g_connected;
    status->hook_active = false;
    status->api_version = ZYGISK_NEXT_API_VERSION_1;
}