#!/usr/bin/env bash
# vk-diagnose — comprehensive VietKey diagnostic tool
set -euo pipefail

REPORT="vietkey-diagnostic-$(date +%Y%m%d-%H%M%S).txt"
ISSUES=0

log() { echo "$*" | tee -a "$REPORT"; }
issue() { ISSUES=$((ISSUES + 1)); log "  [ISSUE $ISSUES] $*"; }
ok() { log "  [OK] $*"; }
fix() { log "  [FIX] $*"; }

check_env_vars() {
    log "=== Environment Variables ==="
    for var in GTK_IM_MODULE QT_IM_MODULE XMODIFIERS SDL_IM_MODULE; do
        val="${!var:-}"
        if [ -z "$val" ]; then
            issue "$var is not set"
            fix "Add to ~/.bashrc: export $var=fcitx (or ibus for GNOME)"
        else
            ok "$var=$val"
        fi
    done
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

check_libvietkey() {
    log "=== libvietkey.so ==="
    if ldconfig -p 2>/dev/null | grep -q libvietkey; then
        ok "libvietkey.so found in ldconfig cache"
        ldd "$(ldconfig -p | grep libvietkey | awk '{print $NF}' | head -1)" 2>&1 | tee -a "$REPORT"
    elif [ -f /usr/lib/libvietkey.so ]; then
        ok "Found /usr/lib/libvietkey.so"
        ldd /usr/lib/libvietkey.so 2>&1 | tee -a "$REPORT"
    else
        issue "libvietkey.so not found"
        fix "Install VietKey: sudo cmake --install build"
    fi
}

check_vkd_socket() {
    log "=== vkd Daemon ==="
    local sock="/run/user/$(id -u)/vietkey.sock"
    if [ -S "$sock" ]; then
        ok "Socket exists: $sock"
        if echo '{"type":"stats"}' | nc -U "$sock" 2>/dev/null; then
            ok "Socket responds to queries"
        else
            issue "Socket exists but does not respond"
            fix "Restart daemon: systemctl --user restart vkd"
        fi
    else
        issue "vkd socket not found at $sock"
        fix "Start daemon: systemctl --user start vkd"
    fi
}

check_fonts() {
    log "=== Vietnamese Font Coverage ==="
    if command -v fc-match >/dev/null 2>&1; then
        local font
        font=$(fc-match -s ":lang=vi" 2>/dev/null | head -1)
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
    local data_dir="/usr/share/vietkey"
    for f in dict/vi.dict ngram/vi.arpa emoji/emoji.json; do
        if [ -f "$data_dir/$f" ]; then
            ok "$data_dir/$f exists"
        else
            issue "Missing: $data_dir/$f"
            fix "Rebuild data: cd data && ./dict/build_dict.sh"
        fi
    done
}

check_gtk_im_module() {
    log "=== GTK IM Module Compatibility ==="
    local de="${XDG_CURRENT_DESKTOP:-unknown}"
    local gtk_im="${GTK_IM_MODULE:-}"
    case "$de" in
        *GNOME*|*gnome*)
            if [ "$gtk_im" != "ibus" ]; then
                issue "GNOME detected but GTK_IM_MODULE=$gtk_im (expected ibus)"
                fix "export GTK_IM_MODULE=ibus"
            else ok "GTK_IM_MODULE=ibus for GNOME"
            fi ;;
        *)
            if [ "$gtk_im" != "fcitx" ]; then
                issue "Non-GNOME DE but GTK_IM_MODULE=$gtk_im (expected fcitx)"
                fix "export GTK_IM_MODULE=fcitx"
            else ok "GTK_IM_MODULE=fcitx"
            fi ;;
    esac
}

main() {
    log "VietKey Diagnostic Report"
    log "Generated: $(date)"
    log "Hostname: $(hostname)"
    log "User: $(whoami)"
    log ""

    check_env_vars
    check_imf_running
    check_libvietkey
    check_vkd_socket
    check_fonts
    check_wayland
    check_data_files
    check_gtk_im_module

    log ""
    log "=== Summary ==="
    log "Issues found: $ISSUES"
    log "Report saved to: $REPORT"

    if [ "$ISSUES" -gt 0 ]; then
        log "Run vk-setup to fix common issues automatically."
        exit 1
    fi
    exit 0
}

main "$@"
