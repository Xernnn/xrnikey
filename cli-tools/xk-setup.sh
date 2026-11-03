#!/usr/bin/env bash
# xk-setup — interactive Xrnikey setup script
set -euo pipefail

XRNIKEY_SHARE="${XRNIKEY_SHARE:-/usr/share/xrnikey}"
BACKUP_DIR="${HOME}/.local/share/xrnikey/setup-backup"
STATE_DIR="${HOME}/.local/share/xrnikey"
CONFIG_DIR="${HOME}/.config/xrnikey"

log() { echo "[xk-setup] $*"; }
warn() { echo "[xk-setup] WARNING: $*" >&2; }

detect_display() {
    if [ -n "${WAYLAND_DISPLAY:-}" ]; then echo "wayland"
    elif [ -n "${DISPLAY:-}" ]; then echo "x11"
    else echo "unknown"; fi
}

detect_de() {
    if [ -n "${XDG_CURRENT_DESKTOP:-}" ]; then
        echo "$XDG_CURRENT_DESKTOP" | tr '[:upper:]' '[:lower:]'
    elif [ -n "${DESKTOP_SESSION:-}" ]; then
        echo "$DESKTOP_SESSION" | tr '[:upper:]' '[:lower:]'
    else echo "unknown"; fi
}

detect_shell() {
    basename "${SHELL:-/bin/bash}"
}

detect_imf() {
    if pgrep -x fcitx5 >/dev/null 2>&1; then echo "fcitx5"
    elif pgrep -x ibus-daemon >/dev/null 2>&1; then echo "ibus"
    else echo "none"; fi
}

backup_file() {
    local f="$1"
    [ -f "$f" ] || return 0
    mkdir -p "$BACKUP_DIR"
    cp -a "$f" "$BACKUP_DIR/$(basename "$f").$(date +%Y%m%d%H%M%S)"
}

rollback() {
    log "Rolling back from $BACKUP_DIR..."
    if [ ! -d "$BACKUP_DIR" ]; then
        warn "No backup directory found"; exit 1
    fi
    for f in "$BACKUP_DIR"/*; do
        orig_name=$(basename "$f" | sed 's/\.[0-9]*$//')
        case "$orig_name" in
            bashrc) cp "$f" "$HOME/.bashrc" ;;
            zshrc) cp "$f" "$HOME/.zshrc" ;;
            pam_environment) cp "$f" "$HOME/.pam_environment" ;;
            *) warn "Unknown backup: $f" ;;
        esac
    done
    log "Rollback complete."
    exit 0
}

setup_env_vars() {
    local imf="$1"
    local im_module
    if [ "$imf" = "fcitx5" ]; then im_module="fcitx"
    else im_module="ibus"; fi

    local env_block="
# Xrnikey IME environment
export GTK_IM_MODULE=${im_module}
export QT_IM_MODULE=${im_module}
export XMODIFIERS=@im=${im_module}
export SDL_IM_MODULE=${im_module}
"

    local shell
    shell=$(detect_shell)
    case "$shell" in
        bash)
            backup_file "$HOME/.bashrc"
            echo "$env_block" >> "$HOME/.bashrc"
            log "Added environment variables to ~/.bashrc"
            ;;
        zsh)
            backup_file "$HOME/.zshrc"
            echo "$env_block" >> "$HOME/.zshrc"
            log "Added environment variables to ~/.zshrc"
            ;;
        fish)
            backup_file "$HOME/.config/fish/config.fish"
            mkdir -p "$HOME/.config/fish"
            cat >> "$HOME/.config/fish/config.fish" <<FISH
set -gx GTK_IM_MODULE ${im_module}
set -gx QT_IM_MODULE ${im_module}
set -gx XMODIFIERS @im=${im_module}
set -gx SDL_IM_MODULE ${im_module}
FISH
            log "Added environment variables to fish config"
            ;;
    esac
}

setup_autostart() {
    local imf="$1"
    mkdir -p "$STATE_DIR" "$CONFIG_DIR"
    mkdir -p "$HOME/.config/systemd/user"

    # xkd daemon: not enabled. Neither the Fcitx5 addon nor the IBus engine queries it yet (both
    # read the dictionary in process), so it would only sit idle. To try its socket API by hand:
    #   systemctl --user start xkd.service

    if [ "$imf" = "fcitx5" ]; then
        mkdir -p "$HOME/.config/autostart"
        cat > "$HOME/.config/autostart/fcitx5.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Fcitx5
Exec=fcitx5 -d
Comment=Input Method Framework
DESKTOP
        log "Created Fcitx5 autostart entry"
    fi
}

fix_electron_apps() {
    log "Scanning for Electron apps..."
    local apps=("Code" "slack" "discord" "obsidian" "cursor")
    for app in "${apps[@]}"; do
        local conf_dir="$HOME/.config/$app"
        if [ -d "$conf_dir" ]; then
            local flags_file="$conf_dir/flags.conf"
            if [ -f "$flags_file" ]; then
                if ! grep -q "gtk-version=4" "$flags_file" 2>/dev/null; then
                    backup_file "$flags_file"
                    echo "--gtk-version=4" >> "$flags_file"
                    log "Patched $app flags.conf with --gtk-version=4"
                fi
            fi
        fi
    done
}

install_imf() {
    local de="$1"
    case "$de" in
        *gnome*)
            log "GNOME detected — configuring IBus"
            if command -v pacman >/dev/null 2>&1; then
                sudo pacman -S --needed ibus python-gobject 2>/dev/null || true
            fi
            setup_env_vars "ibus"
            setup_autostart "ibus"
            ;;
        *)
            log "Non-GNOME DE — configuring Fcitx5"
            if command -v pacman >/dev/null 2>&1; then
                sudo pacman -S --needed fcitx5 fcitx5-gtk fcitx5-qt fcitx5-configtool 2>/dev/null || true
            fi
            setup_env_vars "fcitx5"
            setup_autostart "fcitx5"
            ;;
    esac
}

validate_setup() {
    log ""
    log "=== Validation ==="
    log "Display server: $(detect_display)"
    log "Desktop: $(detect_de)"
    log "IMF: $(detect_imf)"
    log ""
    log "Please type 'xin chào' in any text field to verify Xrnikey works."
    log "Switch input method with Ctrl+Space (Fcitx5) or Super+Space (IBus)."
}

main() {
    if [ "${1:-}" = "--rollback" ]; then rollback; fi

    log "Xrnikey Setup v1.0.0"
    log "===================="

    local display de shell imf
    display=$(detect_display)
    de=$(detect_de)
    shell=$(detect_shell)
    imf=$(detect_imf)

    log "Display: $display"
    log "Desktop: $de"
    log "Shell: $shell"
    log "Current IMF: $imf"

    mkdir -p "$STATE_DIR" "$CONFIG_DIR"

    # Default config files
    if [ ! -f "$STATE_DIR/state.json" ]; then
        echo '{"enabled": true}' > "$STATE_DIR/state.json"
    fi
    if [ ! -f "$CONFIG_DIR/app-overrides.json" ]; then
        echo '["code","alacritty","kitty","foot","wezterm","terminal","konsole"]' > "$CONFIG_DIR/app-overrides.json"
    fi

    install_imf "$de"
    fix_electron_apps
    validate_setup

    log "Setup complete! Log out and back in for environment changes to take effect."
}

main "$@"
