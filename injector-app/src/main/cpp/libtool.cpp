// libtool - 被注入的库
#include <android/log.h>
#include <jni.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>

#define LOG_TAG "never"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

// 入口函数 - 注入后调用
JNIEXPORT void JNICALL
Java_com_injector_app_MainActivity_onLoad(JNIEnv* env, jobject thiz) {
    LOGI("=== LibTool Loaded ===");
    LOGI("PID: %d", getpid());
    LOGI("UID: %d", getuid());

    // 延迟初始化
    sleep(2);

    // 检查依赖
    void* libc = dlopen("libc.so", RTLD_NOW);
    if (libc) {
        LOGI("libc.so: OK");
        dlclose(libc);
    } else {
        LOGE("libc.so: FAILED");
    }

    LOGI("never init complete!");
}

// JNI_OnLoad
JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    LOGI("JNI_OnLoad");
    return JNI_VERSION_1_6;
}