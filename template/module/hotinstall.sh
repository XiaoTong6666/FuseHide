#!/system/bin/sh
# Runs after KernelSU has atomically activated the replacement module directory.
# Do not touch an already running MediaProvider: a live FUSE session can hold
# file descriptors and cached dentries, and a Zygisk implementation may still
# refer to the old module directory until Zygote itself restarts.

echo "- FuseHide module files are hot-installed."
echo "- Running Zygisk/MediaProvider code is unchanged until it is reloaded."
echo "- Restart MediaProvider manually to test the new runtime, or reboot if needed."
exit 0
