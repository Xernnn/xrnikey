#!/usr/bin/env bash
# Runs the real fcitx5 binary with the freshly built addon and types into it
# over its D-Bus input-context API (the one the fcitx5 GTK/Qt modules use),
# so the installed layout, addon loading, config reload and the D-Bus
# frontend are covered, not just the engine logic.
#
# Isolated from the desktop it runs on: a private D-Bus daemon with no
# service activation on an abstract socket, no DISPLAY/WAYLAND_DISPLAY, and
# throwaway HOME/XDG dirs, so it neither sees nor touches a running fcitx5.
#
# usage: dbus_session.sh <addon build dir> <fcitx5 data dir> <xrnikey data dir> <work dir> <driver.py>
# exits 77 (skipped) when fcitx5, dbus-daemon or python3-gi is missing.
set -u
ADDON_DIR=$1 FCITX_DATA=$2 XK_DATA=$3 WORK=$4 DRIVER=$5

# The system's tools, not a virtualenv's python3 that may come first in PATH.
export PATH=/usr/local/bin:/usr/bin:/bin
command -v fcitx5 >/dev/null || { echo "fcitx5 not installed: skipped"; exit 77; }
command -v dbus-daemon >/dev/null || { echo "dbus-daemon not installed: skipped"; exit 77; }
python3 -c 'import gi; gi.require_version("Gio", "2.0")' 2>/dev/null ||
    { echo "python3-gi not installed: skipped"; exit 77; }
SYS_ADDONS=
for d in /usr/lib/fcitx5 /usr/lib/*/fcitx5 /usr/lib64/fcitx5; do
    [ -e "$d/libdbusfrontend.so" ] && SYS_ADDONS=$d && break
done
[ -n "$SYS_ADDONS" ] || { echo "fcitx5 D-Bus frontend not found: skipped"; exit 77; }

T=$(mktemp -d "$WORK/dbus-session-XXXXXX")
SOCK="xrnikey-addon-test-$$-$RANDOM"
mkdir -p "$T/run" "$T/home/.config/fcitx5/conf" "$T/home/.local/share"
chmod 700 "$T/run"
cat > "$T/bus.conf" <<CONF
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:abstract=$SOCK</listen>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/><allow eavesdrop="true"/><allow own="*"/>
  </policy>
</busconfig>
CONF
cat > "$T/home/.config/fcitx5/profile" <<PROF
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=xrnikey

[Groups/0/Items/0]
Name=keyboard-us
Layout=

[Groups/0/Items/1]
Name=xrnikey
Layout=

[GroupOrder]
0=Default
PROF
: > "$T/home/.config/fcitx5/conf/xrnikey5.conf"

ENVS=(PATH="$PATH" HOME="$T/home" XDG_RUNTIME_DIR="$T/run"
      XDG_CONFIG_HOME="$T/home/.config" XDG_DATA_HOME="$T/home/.local/share"
      XDG_DATA_DIRS="$FCITX_DATA:/usr/local/share:/usr/share"
      FCITX_ADDON_DIRS="$ADDON_DIR:$SYS_ADDONS" XRNIKEY_DATA_DIR="$XK_DATA"
      DBUS_SESSION_BUS_ADDRESS="unix:abstract=$SOCK")

env -i "${ENVS[@]}" dbus-daemon --config-file="$T/bus.conf" --nofork --nopidfile &
BUS=$!
cleanup() {
    [ -n "${FX:-}" ] && kill "$FX" 2>/dev/null
    sleep 0.2
    [ -n "${FX:-}" ] && kill -9 "$FX" 2>/dev/null
    kill "$BUS" 2>/dev/null
    wait 2>/dev/null
    rm -rf "$T"
}
trap cleanup EXIT
sleep 0.3
env -i "${ENVS[@]}" fcitx5 --disable=all --enable=keyboard,dbus,dbusfrontend,xrnikey5 \
    > "$T/fcitx5.log" 2>&1 &
FX=$!
env -i "${ENVS[@]}" DRIVER_CONF="$T/home/.config/fcitx5/conf/xrnikey5.conf" \
    timeout 60 python3 -I "$DRIVER"
RC=$?
if [ "$RC" -ne 0 ]; then
    echo "--- fcitx5 log"
    cat "$T/fcitx5.log"
fi
exit "$RC"
