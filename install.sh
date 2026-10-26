#!/usr/bin/env bash
set -euo pipefail

REPO_RAW_URL="https://raw.githubusercontent.com/Korayami/Seeker/main"

# Text formatting
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

echo -e "${BOLD}${BLUE}==> Installing seeker & seekui...${NC}"

# Find or prepare source files (seek.c & seekui.c)
TMP_DIR=""
cleanup() {
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT

SEEK_SRC=""
SEEKUI_SRC=""

# Check if sources exist locally (e.g. running from cloned repo)
if [ -f "seek.c" ]; then
    SEEK_SRC="$(pwd)/seek.c"
    [ -f "seekui.c" ] && SEEKUI_SRC="$(pwd)/seekui.c"
elif [ -n "${BASH_SOURCE[0]:-}" ] && [ -f "$(dirname "${BASH_SOURCE[0]:-.}")/seek.c" ]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" 2>/dev/null && pwd || true)"
    if [ -n "$SCRIPT_DIR" ] && [ -f "$SCRIPT_DIR/seek.c" ]; then
        SEEK_SRC="$SCRIPT_DIR/seek.c"
        [ -f "$SCRIPT_DIR/seekui.c" ] && SEEKUI_SRC="$SCRIPT_DIR/seekui.c"
    fi
fi

# Download missing files if piped from curl/wget
if [ -z "$SEEK_SRC" ] || [ -z "$SEEKUI_SRC" ]; then
    TMP_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t 'seeker')"
    echo -e "Downloading source from GitHub (${REPO_RAW_URL})..."
    
    download() {
        local file="$1"
        if command -v curl >/dev/null 2>&1; then
            curl -fsSL "${REPO_RAW_URL}/$file" -o "$TMP_DIR/$file"
        elif command -v wget >/dev/null 2>&1; then
            wget -q "${REPO_RAW_URL}/$file" -O "$TMP_DIR/$file"
        else
            echo -e "${RED}Error: Neither curl nor wget was found.${NC}" >&2
            exit 1
        fi
    }
    
    [ -z "$SEEK_SRC" ] && download "seek.c" && SEEK_SRC="$TMP_DIR/seek.c"
    [ -z "$SEEKUI_SRC" ] && download "seekui.c" && SEEKUI_SRC="$TMP_DIR/seekui.c"
fi

# Detect C compiler (prefer gcc on Linux, clang on macOS)
CC="${CC:-$(command -v gcc || command -v clang || command -v cc || true)}"

if [ -z "$CC" ]; then
    echo -e "${YELLOW}Notice: No C compiler found.${NC}"
    if command -v apt-get >/dev/null 2>&1; then
        echo -e "Installing gcc via apt..."
        if [ "$(id -u)" -eq 0 ]; then
            apt-get update && apt-get install -y gcc make
        elif command -v sudo >/dev/null 2>&1; then
            sudo apt-get update && sudo apt-get install -y gcc make
        fi
        CC="$(command -v gcc || command -v cc)"
    fi
fi

if [ -z "$CC" ]; then
    echo -e "${RED}Error: Could not find or install a C compiler (gcc, clang, or cc).${NC}" >&2
    exit 1
fi

echo -e "Compiling CLI and TUI with: ${BOLD}$CC -O3${NC}"
BUILD_DIR="${TMP_DIR:-$(pwd)}"
$CC -O3 -Wall -Wextra -pedantic "$SEEK_SRC" -o "$BUILD_DIR/seeker"
$CC -O3 -Wall -Wextra -pedantic "$SEEKUI_SRC" -o "$BUILD_DIR/seekui"
echo -e "${GREEN}✓ Build succeeded for seeker and seekui.${NC}"

# Determine installation directory
TARGET_DIR="/usr/local/bin"

if [ "${1:-}" = "--user" ] || [ "${1:-}" = "-u" ]; then
    TARGET_DIR="$HOME/.local/bin"
elif [ -n "${1:-}" ] && [[ "${1:-}" != -* ]]; then
    TARGET_DIR="$1"
fi

mkdir -p "$TARGET_DIR" 2>/dev/null || true

echo -e "${BOLD}${BLUE}==> Installing to $TARGET_DIR...${NC}"

install_bins() {
    local dir="$1"
    mkdir -p "$dir"
    cp -f "$BUILD_DIR/seeker" "$dir/seeker"
    cp -f "$BUILD_DIR/seekui" "$dir/seekui"
    chmod 755 "$dir/seeker" "$dir/seekui"
    ln -sf "$dir/seeker" "$dir/seek"
}

if [ -w "$TARGET_DIR" ] || [ "$(id -u)" -eq 0 ]; then
    install_bins "$TARGET_DIR"
else
    if command -v sudo >/dev/null 2>&1; then
        echo -e "${YELLOW}Elevated permissions required for $TARGET_DIR. Using sudo...${NC}"
        sudo mkdir -p "$TARGET_DIR"
        sudo cp -f "$BUILD_DIR/seeker" "$TARGET_DIR/seeker"
        sudo cp -f "$BUILD_DIR/seekui" "$TARGET_DIR/seekui"
        sudo chmod 755 "$TARGET_DIR/seeker" "$TARGET_DIR/seekui"
        sudo ln -sf "$TARGET_DIR/seeker" "$TARGET_DIR/seek"
    else
        echo -e "${YELLOW}Cannot write to $TARGET_DIR without sudo. Installing to $HOME/.local/bin instead...${NC}"
        TARGET_DIR="$HOME/.local/bin"
        install_bins "$TARGET_DIR"
    fi
fi

echo ""
echo -e "${BOLD}${GREEN}==> Successfully installed 'seek', 'seeker', and 'seekui' to $TARGET_DIR!${NC}"

# Check PATH
if [[ ":$PATH:" != *":$TARGET_DIR:"* ]]; then
    echo -e "${YELLOW}Note: '$TARGET_DIR' is not in your current PATH.${NC}"
    echo -e "Add it to your ~/.bashrc or ~/.zshrc:"
    echo -e "    ${BOLD}export PATH=\"$TARGET_DIR:\$PATH\"${NC}"
fi

echo -e "Available commands:"
echo -e "  • ${BOLD}seek \"pattern\"${NC}   (CLI search tool)"
echo -e "  • ${BOLD}seekui${NC}           (Interactive Visual TUI App)"
