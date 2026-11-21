#!/usr/bin/env bash
# xk-diagnose — comprehensive xrnkey diagnostic tool
#
#   xk-diagnose                the report, on standard output
#   xk-diagnose --output PATH  also write it to PATH
#   xk-diagnose --full         include the host and user name
set -euo pipefail

REPORT=""
FULL=false
ISSUES=0

log() {
    echo "$*"
    if [ -n "$REPORT" ]; then echo "$*" >> "$REPORT"; fi
}
# Command output into the report, line by line.
log_lines() { local line; while IFS= read -r line; do log "$line"; done; }
issue() { ISSUES=$((ISSUES + 1)); log "  [ISSUE $ISSUES] $*"; }
ok() { log "  [OK] $*"; }
fix() { log "  [FIX] $*"; }

check_env_vars() {
    log "=== Environment Variables ==="
    # On Wayland GTK apps reach Fcitx5 through text-input-v3, and the Fcitx5
    # wiki says to leave GTK_IM_MODULE unset there; the others are still
    # needed (Qt, SDL, and X11 apps under XWayland).
    for var in QT_IM_MODULE XMODIFIERS SDL_IM_MODULE GTK_IM_MODULE; do
        val="${!var:-}"
        if [ "$var" = GTK_IM_MODULE ] && [ -n "${WAYLAND_DISPLAY:-}" ]; then
            if [ -z "$val" ]; then
                ok "GTK_IM_MODULE unset (right for Wayland)"
            else
                log "  [NOTE] GTK_IM_MODULE=$val: on Wayland the Fcitx5 wiki says to leave it unset"
            fi
        elif [ -z "$val" ]; then
            issue "$var is not set"
            fix "Set $var=fcitx where your session gets its environment (xk-setup can do it)"
        else
            ok "$var=$val"
        fi
    done
}

check_xrnkey_installed() {
    log "=== xrnkey in Fcitx5 ==="
    local found=false f
    for f in /usr/lib/fcitx5/libxrnkey.so /usr/lib/*/fcitx5/libxrnkey.so; do
        [ -e "$f" ] && { ok "Addon: $f"; found=true; break; }
    done
    $found || { issue "libxrnkey.so is not in Fcitx5's addon directory"
                fix "Install with -DCMAKE_INSTALL_PREFIX=/usr (see README)"; }
    for f in /usr/share/fcitx5/addon/xrnkey.conf /usr/share/fcitx5/inputmethod/xrnkey.conf; do
        if [ -e "$f" ]; then ok "$f"; else issue "Missing: $f"; fi
    done
    local profile="${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/profile"
    if [ -f "$profile" ] && grep -q -x "Name=xrnkey" "$profile"; then
        ok "xrnkey is in the Fcitx5 input method list"
    else
        issue "xrnkey is not in the Fcitx5 input method list"
        fix "Add it in fcitx5-configtool, or see README, \"Switching from Bamboo\""
    fi
}

check_imf_running() {
    log "=== Input Method Framework ==="
    local fcitx5_running=false ibus_running=false
    pgrep -x fcitx5 >/dev/null 2>&1 && fcitx5_running=true
    pgrep -x ibus-daemon >/dev/null 2>&1 && ibus_running=true

    if $fcitx5_running && $ibus_running; then
        issue "Both Fcitx5 and IBus are running — this causes conflicts"
        fix "Stop one: killall ibus-daemon OR killall fcitx5"
    elif $fcitx5_running; then ok "Fcitx5 is running"
    elif $ibus_running; then ok "IBus is running"
    else
        issue "No input method framework is running"
        fix "Start Fcitx5: fcitx5 -d  OR  IBus: ibus-daemon -drx"
    fi
}

check_libxrnkey() {
    log "=== libxrnkey.so ==="
    if ldconfig -p 2>/dev/null | grep -q libxrnkey; then
        ok "libxrnkey.so found in ldconfig cache"
        ldd "$(ldconfig -p | grep libxrnkey | awk '{print $NF}' | head -1)" 2>&1 | log_lines
    elif [ -f /usr/lib/libxrnkey.so ]; then
        ok "Found /usr/lib/libxrnkey.so"
        ldd /usr/lib/libxrnkey.so 2>&1 | log_lines
    else
        issue "libxrnkey.so not found"
        fix "Install xrnkey: sudo cmake --install build"
    fi
}

check_fonts() {
    log "=== Vietnamese Font Coverage ==="
    if command -v fc-match >/dev/null 2>&1; then
        local font
        # head closing the pipe early must not end the script (pipefail).
        font=$(fc-match -s ":lang=vi" 2>/dev/null | head -1 || true)
        if [ -n "$font" ]; then
            ok "Vietnamese font: $font"
        else
            issue "No font with Vietnamese coverage found"
            fix "Install: sudo pacman -S noto-fonts noto-fonts-cjk"
        fi
    else
        log "  fc-match not available, skipping font check"
    fi
}

check_wayland() {
    log "=== Wayland/X11 ==="
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then
        ok "Wayland session detected"
        if [ "${XDG_SESSION_TYPE:-}" = "wayland" ]; then
            ok "XDG_SESSION_TYPE=wayland"
        else
            issue "WAYLAND_DISPLAY set but XDG_SESSION_TYPE is not wayland"
        fi
    elif [ -n "${DISPLAY:-}" ]; then
        ok "X11 session detected (DISPLAY=$DISPLAY)"
    else
        issue "No display server detected"
    fi
}

check_data_files() {
    log "=== Data Files ==="
    local data_dir="/usr/share/xrnkey"
    for f in dict/vi.dict ngram/vi.arpa emoji/emoji.json; do
        if [ -f "$data_dir/$f" ]; then
            ok "$data_dir/$f exists"
        else
            issue "Missing: $data_dir/$f"
            fix "Rebuild data: cd data && ./dict/build_dict.sh"
        fi
    done
}

main() {
    while [ $# -gt 0 ]; do
        case "$1" in
            --output)
                [ $# -ge 2 ] || { echo "xk-diagnose: --output needs a path" >&2; exit 2; }
                REPORT="$2"; shift 2 ;;
            --full) FULL=true; shift ;;
            *) echo "usage: xk-diagnose [--full] [--output PATH]" >&2; exit 2 ;;
        esac
    done
    if [ -n "$REPORT" ]; then : > "$REPORT"; fi

    log "xrnkey Diagnostic Report"
    log "Generated: $(date)"
    if $FULL; then
        log "Hostname: $(hostname)"
        log "User: $(whoami)"
    fi
    log ""

    check_env_vars
    check_imf_running
    check_libxrnkey
    check_xrnkey_installed
    check_fonts
    check_wayland
    check_data_files

    log ""
    log "=== Summary ==="
    log "Issues found: $ISSUES"
    if [ -n "$REPORT" ]; then echo "Report saved to: $REPORT"; fi

    if [ "$ISSUES" -gt 0 ]; then
        log "Run xk-setup to fix common issues automatically."
        exit 1
    fi
    exit 0
}

main "$@"
