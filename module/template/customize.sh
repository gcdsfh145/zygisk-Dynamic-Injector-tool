# shellcheck disable=SC2034
SKIPUNZIP=1

DEBUG=@DEBUG@
SONAME=@SONAME@
SUPPORTED_ABIS="@SUPPORTED_ABIS@"

if [ "$BOOTMODE" ] && [ "$KSU" ]; then
  ui_print "- Installing from KernelSU app"
  ui_print "- KernelSU version: $KSU_KERNEL_VER_CODE (kernel) + $KSU_VER_CODE (ksud)"
  if [ "$(which magisk)" ]; then
    ui_print "*********************************************************"
    ui_print "! Multiple root implementation is NOT supported!"
    ui_print "! Please uninstall Magisk before installing $SONAME"
    abort    "*********************************************************"
  fi
elif [ "$BOOTMODE" ] && [ "$MAGISK_VER_CODE" ]; then
  ui_print "- Installing from Magisk app"
else
  ui_print "*********************************************************"
  ui_print "! Install from recovery is not supported"
  ui_print "! Please install from KernelSU or Magisk app"
  abort    "*********************************************************"
fi

VERSION=$(grep_prop version "${TMPDIR}/module.prop")
ui_print "- Installing $SONAME $VERSION"

# check architecture
support=false
for abi in $SUPPORTED_ABIS
do
  if [ "$ARCH" == "$abi" ]; then
    support=true
  fi
done
if [ "$support" == "false" ]; then
  abort "! Unsupported platform: $ARCH"
else
  ui_print "- Device platform: $ARCH"
fi

ui_print "- Extracting verify.sh"
unzip -o "$ZIPFILE" 'verify.sh' -d "$TMPDIR" >&2
if [ ! -f "$TMPDIR/verify.sh" ]; then
  ui_print "*********************************************************"
  ui_print "! Unable to extract verify.sh!"
  ui_print "! This zip may be corrupted, please try downloading again"
  abort    "*********************************************************"
fi
. "$TMPDIR/verify.sh"
extract "$ZIPFILE" 'customize.sh'  "$TMPDIR/.vunzip"
extract "$ZIPFILE" 'verify.sh'     "$TMPDIR/.vunzip"
extract "$ZIPFILE" 'sepolicy.rule' "$MODPATH"

ui_print "- Extracting module files"
extract "$ZIPFILE" 'module.prop'     "$MODPATH"
extract "$ZIPFILE" 'post-fs-data.sh' "$MODPATH"
extract "$ZIPFILE" 'service.sh'      "$MODPATH"
extract "$ZIPFILE" 'zn_modules.txt'  "$MODPATH"
mv "$TMPDIR/sepolicy.rule" "$MODPATH"

mkdir "$MODPATH/lib"

ui_print "- Extracting $ARCH libraries"
extract "$ZIPFILE" "lib/$ARCH/lib$SONAME.so" "$MODPATH/lib" true

# 创建默认注入目标配置文件
ui_print "- Setting up injection targets"
INJECT_TARGETS_FILE="/data/local/tmp/zn_inject_targets.txt"
if [ ! -f "$INJECT_TARGETS_FILE" ]; then
  # 如果模块目录有默认配置，使用它
  if [ -f "$MODPATH/zn_modules.txt" ]; then
    cp "$MODPATH/zn_modules.txt" "$INJECT_TARGETS_FILE"
    ui_print "- Copied default inject targets"
  else
    # 创建空配置文件
    echo "# Zygisk Injector - 注入目标配置" > "$INJECT_TARGETS_FILE"
    echo "# 格式: 包名:SO路径:入口函数" >> "$INJECT_TARGETS_FILE"
    echo "# 示例: com.example.game:/data/local/tmp/libhack.so:JNI_OnLoad" >> "$INJECT_TARGETS_FILE"
    ui_print "- Created empty inject targets file"
  fi
  chmod 666 "$INJECT_TARGETS_FILE"
fi

ui_print ""
ui_print "- Installation complete!"
ui_print "- Edit $INJECT_TARGETS_FILE to configure injection targets"
ui_print ""