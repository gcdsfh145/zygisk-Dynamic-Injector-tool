# Zygisk Injector - Service 脚本
# 在系统启动后持续运行

DEBUG=@DEBUG@
MODDIR=${0%/*}

# 设置环境变量
export ZN_INJECT_TARGETS="/data/local/tmp/zn_inject_targets.txt"

# 监控日志
log -p i -t zygisk-injector "Service started"
log -p i -t zygisk-injector "Inject targets: $ZN_INJECT_TARGETS"

# 如果有注入目标配置，显示它们
if [ -f "$ZN_INJECT_TARGETS" ]; then
  COUNT=$(grep -v '^#' "$ZN_INJECT_TARGETS" | grep -v '^$' | wc -l)
  log -p i -t zygisk-injector "Configured targets: $COUNT"
fi