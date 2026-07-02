#!/usr/bin/env bash
# VietKey universal installer for non-Arch distros
set -euo pipefail

VIETKEY_VERSION="1.0.0"
INSTALL_PREFIX="/usr/local"
BUILD_DIR="/tmp/vietkey-build"

log() { echo "[vietkey-install] $*"; }

detect_distro() {
    if [ -f /etc/arch-release ]; then echo "arch"
    elif [ -f /etc/debian_version ]; then echo "debian"
    elif [ -f /etc/fedora-release ]; then echo "fedora"
    elif [ -f /etc/SUSE-brand ]; then echo "suse"
    else echo "unknown"; fi
}

install_deps() {
    local distro="$1"
    log "Installing build dependencies for $distro..."

    case "$distro" in
        arch)
            sudo pacman -S --needed --noconfirm \
                cmake extra-cmake-modules fcitx5 fcitx5-gtk fcitx5-qt \
                boost python python-gobject libibus catch2 noto-fonts
            ;;
        debian)
            sudo apt-get update
            sudo apt-get install -y \
                cmake g++ libfcitx5core-dev libboost-all-dev \
                libibus-1.0-dev python3 python3-gi \
                fcitx5 fcitx5-frontend-gtk3 fcitx5-frontend-qt5 \
                fonts-noto fonts-noto-cjk
            ;;
        fedora)
            sudo dnf install -y \
                cmake gcc-c++ fcitx5-devel boost-devel \
                libibus-devel python3 python3-gobject \
                fcitx5 fcitx5-gtk fcitx5-qt google-noto-fonts
            ;;
        suse)
            sudo zypper install -y \
                cmake gcc-c++ fcitx5-devel boost-devel \
                libibus-1_0-devel python3 python3-gobject \
                fcitx5 google-noto-fonts
            ;;
        *)
            log "Unknown distro. Install manually: cmake, g++20, fcitx5-dev, boost, python3"
            ;;
    esac
}

build_and_install() {
    local src_dir
    src_dir="$(cd "$(dirname "$0")/.." && pwd)"

    log "Building VietKey from $src_dir..."
    cmake -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
        -DVIETKEY_ENABLE_TESTS=ON \
        -DVIETKEY_DATA_DIR="$INSTALL_PREFIX/share/vietkey" \
        -S "$src_dir"

    cmake --build "$BUILD_DIR" -- -j"$(nproc)"

    log "Running tests..."
    cd "$BUILD_DIR" && ctest --output-on-failure || log "Some tests failed (non-fatal)"

    log "Installing to $INSTALL_PREFIX..."
    sudo cmake --install "$BUILD_DIR"

    sudo ldconfig 2>/dev/null || true
}

run_setup() {
    if command -v vk-setup >/dev/null 2>&1; then
        log "Running vk-setup..."
        vk-setup
    else
        log "vk-setup not found in PATH. Add $INSTALL_PREFIX/bin to PATH."
    fi
}

main() {
    log "VietKey Universal Installer v${VIETKEY_VERSION}"
    log "==========================================="

    local distro
    distro=$(detect_distro)
    log "Detected distro: $distro"

    install_deps "$distro"
    build_and_install
    run_setup

    log ""
    log "Installation complete!"
    log "Log out and back in, then select VietKey as your input method."
    log "Run 'vk-diagnose' to verify the installation."
}

main "$@"
