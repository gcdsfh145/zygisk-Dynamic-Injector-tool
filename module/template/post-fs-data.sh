# Zygisk Injector - Post-fs-data 脚本
# 在文件系统挂载后执行

MODDIR=${0%/*}

# 设置环境变量
export ZN_INJECT_TARGETS="/data/local/tmp/zn_inject_targets.txt"

# 确保目录存在
mkdir -p /data/local/tmp
mkdir -p /data/adb/zygisk

# 设置权限
chmod 666 /data/local/tmp/zn_inject_targets.txt 2>/dev/null

# 加载模块
insmod "$MODDIR/lib/libzygisk-injector.so" 2>/dev/null

# 日志
log -p i -t zygisk-injector "Post-fs-data completed"