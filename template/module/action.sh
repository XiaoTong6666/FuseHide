#!/system/bin/sh
# KernelSU / Magisk Action: confirm with the physical volume-up key before
# restarting MediaProvider. Action has no interactive stdin.

if [ "$(id -u)" != 0 ]; then
  echo "! Root is required."
  exit 1
fi

# Resolve only the two MediaProvider process names used by AOSP/Google.
media_package=
media_pid=
for name in com.android.providers.media.module com.google.android.providers.media.module; do
  current_pid=$(pidof "$name" 2>/dev/null)
  if [ -n "$current_pid" ]; then
    if [ -n "$media_package" ]; then
      echo "! Multiple MediaProvider processes found; refusing to guess."
      exit 1
    fi
    media_package=$name
    media_pid=$current_pid
  fi
done
if [ -z "$media_package" ]; then
  echo "! MediaProvider is not running."
  exit 1
fi

# getevent is non-interactive; a private FIFO lets us stop listening as soon
# as a decision is made, and a watchdog bounds the wait without relying on stdin.
tmp=$(/system/bin/mktemp -d /data/local/tmp/fusehide-action.XXXXXX) || exit 1
reader_pid=
watchdog_pid=
cleanup() {
  [ -z "$reader_pid" ] || kill "$reader_pid" 2>/dev/null || :
  [ -z "$watchdog_pid" ] || kill "$watchdog_pid" 2>/dev/null || :
  /system/bin/rm -rf -- "$tmp"
}
trap cleanup EXIT
trap 'exit 130' HUP INT TERM

if ! /system/bin/mkfifo "$tmp/events"; then
  echo "! Unable to open the key-event listener."
  exit 1
fi

/system/bin/getevent -ql > "$tmp/events" 2>/dev/null &
reader_pid=$!
(
  /system/bin/sleep 20
  kill "$reader_pid" 2>/dev/null || :
) &
watchdog_pid=$!

echo "- MediaProvider: $media_package (PID $media_pid)"
echo "- Press VOLUME UP to restart, VOLUME DOWN to cancel."
echo "- Waiting up to 20 seconds..."

selection=
while IFS= read -r event; do
  case "$event" in
    *EV_KEY*KEY_VOLUMEUP*DOWN*) selection=confirm; break ;;
    *EV_KEY*KEY_VOLUMEDOWN*DOWN*) selection=cancel; break ;;
  esac
done < "$tmp/events"

# Stop the listener before touching MediaProvider.
kill "$reader_pid" 2>/dev/null || :
kill "$watchdog_pid" 2>/dev/null || :
reader_pid=
watchdog_pid=

case "$selection" in
  cancel)
    echo "- Cancelled."
    exit 0
    ;;
  confirm) ;;
  *)
    echo "! No volume-up confirmation received; nothing changed."
    exit 1
    ;;
esac

# Do not force-stop a different process if MediaProvider was restarted during
# the key prompt.
if [ "$(pidof "$media_package" 2>/dev/null)" != "$media_pid" ]; then
  echo "! MediaProvider PID changed while waiting; retry the action."
  exit 1
fi

echo "- Restarting $media_package..."
if ! /system/bin/am force-stop "$media_package"; then
  echo "! Failed to stop MediaProvider."
  exit 1
fi

attempt=0
while [ "$attempt" -lt 20 ]; do
  # File-system access normally starts MediaProvider again on AOSP.
  /system/bin/ls -ld /sdcard/Android >/dev/null 2>&1 || :
  new_pid=$(pidof "$media_package" 2>/dev/null)
  if [ -n "$new_pid" ] && [ "$new_pid" != "$media_pid" ]; then
    echo "- MediaProvider restarted: $media_pid -> $new_pid"
    exit 0
  fi
  attempt=$((attempt + 1))
  /system/bin/sleep 1
done

echo "! MediaProvider did not restart within 20 seconds."
exit 1
