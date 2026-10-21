#!/system/bin/sh
# FuseHide Zygisk module installer.

SKIPUNZIP=1

ui_print "- Extracting verify.sh"
unzip -o "$ZIPFILE" 'verify.sh' -d "$TMPDIR" >&2
[ -f "$TMPDIR/verify.sh" ] || abort "Unable to extract verify.sh"
. "$TMPDIR/verify.sh"

verify_apk_signature "$ZIPFILE"

ui_print "- Extracting util_functions.sh"
extract "$ZIPFILE" 'util_functions.sh' "$TMPDIR"
. "$TMPDIR/util_functions.sh"

print_install_context
check_android_version
check_arch

case "$ARCH" in
  arm64) APK_ABI=arm64-v8a ;;
  arm) APK_ABI=armeabi-v7a ;;
  x64) APK_ABI=x86_64 ;;
  x86) APK_ABI=x86 ;;
esac

ui_print "- Extracting module files"
extract "$ZIPFILE" 'module.prop' "$MODPATH"
extract "$ZIPFILE" 'skip_mount' "$MODPATH"
extract "$ZIPFILE" 'post-fs-data.sh' "$MODPATH"
extract "$ZIPFILE" 'service.sh' "$MODPATH"
extract "$ZIPFILE" 'action.sh' "$MODPATH"
extract "$ZIPFILE" 'hotinstall.sh' "$MODPATH"
extract "$ZIPFILE" 'uninstall.sh' "$MODPATH"
extract "$ZIPFILE" 'sepolicy.rule' "$MODPATH"

ui_print "- Extracting FuseHide native libraries for $APK_ABI"
extract "$ZIPFILE" "lib/$APK_ABI/libfusehide.so" "$MODPATH"
mkdir -p "$MODPATH/zygisk"
unzip -p "$ZIPFILE" "lib/$APK_ABI/libzygisk.so" > "$MODPATH/zygisk/$APK_ABI.so" 2>/dev/null || abort "Failed to extract libzygisk.so"
[ -s "$MODPATH/zygisk/$APK_ABI.so" ] || abort "libzygisk.so is empty"

ui_print "- Extracting injected dex payload"
mkdir -p "$MODPATH/dex"
dex_index=1
while true; do
  if [ "$dex_index" -eq 1 ]; then
    source_dex=classes.dex
    target_dex=injected.dex
  else
    source_dex="classes${dex_index}.dex"
    target_dex="injected${dex_index}.dex"
  fi
  if ! unzip -p "$ZIPFILE" "$source_dex" > "$MODPATH/dex/$target_dex" 2>/dev/null; then
    rm -f "$MODPATH/dex/$target_dex"
    break
  fi
  if [ ! -s "$MODPATH/dex/$target_dex" ]; then
    rm -f "$MODPATH/dex/$target_dex"
    [ "$dex_index" -eq 1 ] && abort "classes.dex is missing or empty"
    break
  fi
  dex_index=$((dex_index + 1))
done
[ "$dex_index" -gt 1 ] || abort "No classes.dex found"
ui_print "- Extracted $((dex_index - 1)) dex file(s)"

ui_print "- Setting permissions"
set_perm_recursive "$MODPATH" 0 0 0755 0644
set_perm "$MODPATH/post-fs-data.sh" 0 0 0755
set_perm "$MODPATH/service.sh" 0 0 0755
set_perm "$MODPATH/action.sh" 0 0 0755
set_perm "$MODPATH/hotinstall.sh" 0 0 0755
set_perm "$MODPATH/uninstall.sh" 0 0 0755

ui_print "FuseHide zygisk module installed"

# KernelSU's built-in hot install swaps module directories without replaying
# Zygote or boot scripts. Only request it for updates whose already-loaded
# Zygisk entrypoint is byte-identical; libfusehide.so and DEX are loaded by
# that entrypoint for each new MediaProvider process.
# A first install, changed Zygisk entrypoint, or changed boot script needs a
# normal reboot. Never request native hot install from old KernelSU, Magisk
# or APatch installers.
active_module=/data/adb/modules/zygisk_fusehide
if [ "${KSU:-false}" = true ] && [ "${KSU_HAS_HOT_INSTALL:-false}" = true ] &&
   [ -f "$active_module/zygisk/$APK_ABI.so" ] &&
   [ ! -e "$active_module/disable" ] &&
   [ ! -e "$active_module/remove" ] &&
   [ ! -e "$active_module/lsp_scope_enabled" ] &&
   [ -f "$MODPATH/skip_mount" ] &&
   [ ! -e "$MODPATH/system" ] &&
   [ ! -e "$MODPATH/system.prop" ] &&
   [ ! -e "$MODPATH/initrc" ] &&
   [ -f "$active_module/post-fs-data.sh" ] &&
   [ -f "$active_module/service.sh" ] &&
   cmp -s "$active_module/zygisk/$APK_ABI.so" "$MODPATH/zygisk/$APK_ABI.so" &&
   cmp -s "$active_module/post-fs-data.sh" "$MODPATH/post-fs-data.sh" &&
   cmp -s "$active_module/service.sh" "$MODPATH/service.sh"; then
  export MODULE_HOT_INSTALL_REQUEST=true
  export MODULE_HOT_RUN_SCRIPT=hotinstall.sh
  ui_print "- Requesting KernelSU hot install (Zygisk loader unchanged)"
else
  ui_print "- Reboot required (hot install unavailable or Zygisk/boot entrypoint changed)"
fi
