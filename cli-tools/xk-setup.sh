#!/usr/bin/env bash
# xk-setup — check and, step by step with your consent, set up Fcitx5 for xrnkey.
#
# It never changes an existing input-method setup on its own: every change is
# shown first and needs a "y"; without a terminal to ask on it only reports.
# It never edits Fcitx5's own profile; it prints the commands for that.
#
#   xk-setup             check, then ask before each change
#   xk-setup --dry-run   only report what it would do
#   xk-setup --yes       make the suggested changes without asking
#   xk-setup --rollback  undo the changes it made (from its backups)
set -euo pipefail

STATE_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/xrnkey"
BACKUP_DIR="$STATE_DIR/setup-backup"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
ENV_FILE="$CONFIG_HOME/environment.d/90-xrnkey.conf"
AUTOSTART_FILE="$CONFIG_HOME/autostart/fcitx5.desktop"
# Files this script may create; a rollback only ever touches these.
MANAGED=("$ENV_FILE" "$AUTOSTART_FILE")
# Until 1.1.0 the project was called xrnikey, and so were these two.
OLD_STATE_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/xrnikey"
OLD_ENV_FILE="$CONFIG_HOME/environment.d/90-xrnikey.conf"

# Qt's plugin folders (Qt 6 and 5; Arch, Fedora, Debian-style multiarch).
# XK_QT_PLUGIN_DIRS, space-separated, replaces the list (for tests).
QT_PLUGIN_DIRS=${XK_QT_PLUGIN_DIRS:-/usr/lib/qt6/plugins /usr/lib/qt/plugins /usr/lib64/qt6/plugins /usr/lib64/qt5/plugins /usr/lib/*/qt6/plugins /usr/lib/*/qt5/plugins}

MODE=ask
log() { echo "[xk-setup] $*"; }
warn() { echo "[xk-setup] WARNING: $*" >&2; }

# ask "question": yes in --yes mode, no in --dry-run or without a terminal.
ask() {
    case "$MODE" in
        yes) return 0 ;;
        dry) log "(dry run) would: $1"; return 1 ;;
    esac
    if [ ! -t 0 ]; then
        log "(no terminal to ask on) skipped: $1"
        return 1
    fi
    local reply
    read -r -p "[xk-setup] $1 [y/N] " reply
    [[ "$reply" == [yY]* ]]
}

session_type() {
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then echo wayland
    elif [ -n "${DISPLAY:-}" ]; then echo x11
    else echo unknown; fi
}

running_imf() {
    if pgrep -x fcitx5 >/dev/null 2>&1; then echo fcitx5
    elif pgrep -x ibus-daemon >/dev/null 2>&1; then echo ibus
    else echo none; fi
}

profile_ims() {
    local profile="$CONFIG_HOME/fcitx5/profile"
    [ -f "$profile" ] || return 0
    sed -n 's/^Name=\(.*\)$/\1/p' "$profile" | grep -v -x Default | tr '\n' ' ' || true
}

move_old() {
    if [ "$MODE" = dry ]; then
        log "(dry run) would: move $1 to $2"
    else
        mv "$1" "$2"
    fi
}

# Give what an older xk-setup left under the old name the new one: its
# backups, and the environment file it wrote with its marker, so that
# --rollback still finds them. Only when nothing has the new name yet.
adopt_old_names() {
    local b
    if [ ! -e "$STATE_DIR" ] && [ -d "$OLD_STATE_DIR" ]; then
        move_old "$OLD_STATE_DIR" "$STATE_DIR"
    fi
    if [ ! -e "$ENV_FILE" ] && [ ! -e "$ENV_FILE.xk-setup" ] && [ -f "$OLD_ENV_FILE.xk-setup" ]; then
        if [ -e "$OLD_ENV_FILE" ]; then
            move_old "$OLD_ENV_FILE" "$ENV_FILE"
        fi
        move_old "$OLD_ENV_FILE.xk-setup" "$ENV_FILE.xk-setup"
        for b in "$BACKUP_DIR"/90-xrnikey.conf.*; do
            if [ -e "$b" ]; then
                move_old "$b" "$BACKUP_DIR/90-xrnkey.conf.${b##*/90-xrnikey.conf.}"
            fi
        done
    fi
}

backup_file() {
    local f="$1"
    [ -e "$f" ] || return 0
    mkdir -p "$BACKUP_DIR"
    cp -a "$f" "$BACKUP_DIR/$(basename "$f").$(date +%Y%m%d%H%M%S)"
}

# Undo: put back the newest backup of a file xk-setup replaced, or remove a
# file it created (marked by a .xk-setup file next to it).
rollback() {
    local f base latest
    for f in "${MANAGED[@]}"; do
        [ -f "$f.xk-setup" ] || continue
        base=$(basename "$f")
        latest=$(ls -1 "$BACKUP_DIR/$base".* 2>/dev/null | sort | tail -n 1 || true)
        if [ -n "$latest" ]; then
            cp -a "$latest" "$f"
            log "Restored $f from $latest"
        else
            rm -f "$f"
            log "Removed $f"
        fi
        rm -f "$f.xk-setup"
    done
    log "Rollback complete. Log out and back in for environment changes to take effect."
}

# Whether fcitx5-qt, Fcitx5's input method module for Qt, is installed.
# QT_IM_MODULE=fcitx points Qt apps at it; without it they get no input
# method at all.
qt_module_present() {
    local d
    for d in $QT_PLUGIN_DIRS; do
        [ -e "$d/platforminputcontexts/libfcitx5platforminputcontextplugin.so" ] && return 0
    done
    return 1
}

# What the session needs, per the Fcitx5 wiki ("Using Fcitx 5 on Wayland"): on
# a compositor with an input-method frontend (Hyprland, Sway, KDE) GTK apps
# reach Fcitx5 through text-input-v3, so GTK_IM_MODULE must NOT be set; Qt,
# SDL and X11 (XWayland) apps still need theirs.
wanted_env() {
    if qt_module_present; then
        echo "QT_IM_MODULE=fcitx"
    fi
    echo "XMODIFIERS=@im=fcitx"
    echo "SDL_IM_MODULE=fcitx"
    if [ "$(session_type)" != wayland ]; then
        echo "GTK_IM_MODULE=fcitx"
    fi
}

step_packages() {
    if command -v fcitx5 >/dev/null 2>&1; then
        log "Fcitx5 is installed."
        return
    fi
    warn "Fcitx5 is not installed."
    if command -v pacman >/dev/null 2>&1 &&
        ask "install fcitx5 and fcitx5-configtool with sudo pacman"; then
        sudo pacman -S --needed fcitx5 fcitx5-configtool
    else
        log "Install fcitx5 (and fcitx5-configtool) with your package manager."
    fi
}

step_environment() {
    local missing=() line name value current
    while IFS= read -r line; do
        name=${line%%=*}
        value=${line#*=}
        current=${!name:-}
        if [ "$current" = "$value" ]; then
            log "$name=$value is set."
        elif [ -n "$current" ]; then
            warn "$name is '$current' (Fcitx5 needs '$value'); left as it is."
        else
            missing+=("$line")
        fi
    done < <(wanted_env)
    if ! qt_module_present; then
        if [ "${QT_IM_MODULE:-}" = fcitx ]; then
            warn "QT_IM_MODULE=fcitx is set, but fcitx5-qt (Fcitx5's Qt module) is not installed, so Qt apps get no input method. Install fcitx5-qt, or unset QT_IM_MODULE."
        else
            log "QT_IM_MODULE=fcitx not offered: it needs fcitx5-qt (Fcitx5's Qt module), which is not installed. Install fcitx5-qt for it (on Wayland, Qt apps also reach Fcitx5 without it, through text-input-v3)."
        fi
    fi
    if [ "$(session_type)" = wayland ] && [ -n "${GTK_IM_MODULE:-}" ]; then
        warn "GTK_IM_MODULE=$GTK_IM_MODULE is set; on Wayland the Fcitx5 wiki says to leave it unset."
    fi
    [ "${#missing[@]}" -eq 0 ] && return
    log "Not set: ${missing[*]}"
    log "  (Hyprland: or add 'env = NAME,value' lines to hyprland.conf instead.)"
    if [ -e "$ENV_FILE" ] && [ ! -f "$ENV_FILE.xk-setup" ]; then
        warn "$ENV_FILE exists and xk-setup did not write it; not touching it."
        return
    fi
    if ask "write ${missing[*]} to $ENV_FILE"; then
        backup_file "$ENV_FILE"
        mkdir -p "$(dirname "$ENV_FILE")"
        {
            echo "# Written by xk-setup for Fcitx5/xrnkey; xk-setup --rollback removes it."
            printf '%s\n' "${missing[@]}"
        } > "$ENV_FILE"
        : > "$ENV_FILE.xk-setup"
        log "Wrote $ENV_FILE. Log out and back in for it to take effect."
    fi
}

step_autostart() {
    if [ "$(running_imf)" = fcitx5 ]; then
        log "Fcitx5 is running; how it is started is left alone."
        return
    fi
    if [ -e "$AUTOSTART_FILE" ]; then
        log "$AUTOSTART_FILE exists; left alone."
        return
    fi
    if [ "$(running_imf)" = ibus ]; then
        warn "IBus is running. Run one input method framework: stop IBus before starting Fcitx5."
    fi
    log "  (Hyprland: or add 'exec-once = fcitx5 -d' to hyprland.conf instead.)"
    if ask "create $AUTOSTART_FILE to start Fcitx5 at login"; then
        mkdir -p "$(dirname "$AUTOSTART_FILE")"
        cat > "$AUTOSTART_FILE" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Fcitx5
Exec=fcitx5 -d
Comment=Input Method Framework (written by xk-setup)
DESKTOP
        : > "$AUTOSTART_FILE.xk-setup"
        log "Created $AUTOSTART_FILE."
    fi
}

step_profile() {
    local ims
    ims=$(profile_ims)
    if [[ " $ims " == *" xrnkey "* ]]; then
        log "xrnkey is in your Fcitx5 input methods: $ims"
        return
    fi
    log "xrnkey is not in your Fcitx5 input methods yet${ims:+ (they are: $ims)}."
    log "Add it in fcitx5-configtool, or, with Fcitx5 running, keeping the others (e.g. Bamboo):"
    cat <<'CMDS'
    busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 InputMethodGroupInfo s Default
    # prints e.g.: "us" 2 "keyboard-us" "" "bamboo" ""  -- give the same list back with xrnkey added:
    busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 \
        SetInputMethodGroupInfo 'ssa(ss)' Default us 3 keyboard-us '' bamboo '' xrnkey ''
    busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 Save
    fcitx5-remote -s xrnkey     # use it now; fcitx5-remote -s bamboo goes back
CMDS
}

step_apps() {
    log "Chromium and Electron apps reach Fcitx5 on Wayland only with --enable-wayland-ime"
    log "  (and --ozone-platform=wayland); under XWayland they use XIM. Nothing changed for them."
}

main() {
    case "${1:-}" in
        --rollback) adopt_old_names; rollback; exit 0 ;;
        --dry-run) MODE=dry ;;
        --yes) MODE=yes ;;
        "") ;;
        *) echo "usage: xk-setup [--dry-run | --yes | --rollback]" >&2; exit 2 ;;
    esac
    adopt_old_names

    log "Session: $(session_type), desktop: ${XDG_CURRENT_DESKTOP:-unknown}, running: $(running_imf)"
    if [ "$(running_imf)" != none ] || [ -n "${QT_IM_MODULE:-}${GTK_IM_MODULE:-}${XMODIFIERS:-}" ]; then
        log "An input method setup exists already; nothing in it changes unless you say so."
    fi
    step_packages
    step_environment
    step_autostart
    step_profile
    step_apps
    log "Done. With xrnkey on, 'xin chaof' in a text field should become 'xin chào'."
}

main "$@"
