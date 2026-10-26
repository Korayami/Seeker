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

echo -e "${BOLD}${BLUE}==> Installing seeker...${NC}"

# Find or prepare source file (seek.c)
TMP_DIR=""
cleanup() {
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf "$TMP_DIR"
    fi
}
trap cleanup EXIT

SOURCE_FILE=""
SCRIPT_DIR=""

# Check if seek.c exists locally (e.g. running from cloned repo)
if [ -f "seek.c" ]; then
    SOURCE_FILE="$(pwd)/seek.c"
elif [ -n "${BASH_SOURCE[0]:-}" ] && [ -f "$(dirname "${BASH_SOURCE[0]:-.}")/seek.c" ]; then
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" 2>/dev/null && pwd || true)"
    if [ -n "$SCRIPT_DIR" ] && [ -f "$SCRIPT_DIR/seek.c" ]; then
        SOURCE_FILE="$SCRIPT_DIR/seek.c"
    fi
fi

if [ -z "$SOURCE_FILE" ]; then
    # Download seek.c if piped from curl/wget
    TMP_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t 'seeker')"
    echo -e "Downloading source from GitHub (${REPO_RAW_URL}/seek.c)..."
    if command -v curl >/dev/null 2>&1; then
        curl -fsSL "${REPO_RAW_URL}/seek.c" -o "$TMP_DIR/seek.c"
    elif command -v wget >/dev/null 2>&1; then
        wget -q "${REPO_RAW_URL}/seek.c" -O "$TMP_DIR/seek.c"
    else
        echo -e "${RED}Error: Neither curl nor wget was found. Please install one to proceed.${NC}" >&2
        exit 1
    fi
    SOURCE_FILE="$TMP_DIR/seek.c"
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

echo -e "Compiling with: ${BOLD}$CC -O3${NC}"
BUILD_DIR="${TMP_DIR:-$(pwd)}"
$CC -O3 -Wall -Wextra -pedantic "$SOURCE_FILE" -o "$BUILD_DIR/seeker"
echo -e "${GREEN}✓ Build succeeded.${NC}"

# Determine installation directory
TARGET_DIR="/usr/local/bin"

if [ "${1:-}" = "--user" ] || [ "${1:-}" = "-u" ]; then
    TARGET_DIR="$HOME/.local/bin"
elif [ -n "${1:-}" ] && [[ "${1:-}" != -* ]]; then
    TARGET_DIR="$1"
fi

mkdir -p "$TARGET_DIR" 2>/dev/null || true

echo -e "${BOLD}${BLUE}==> Installing to $TARGET_DIR...${NC}"

if [ -w "$TARGET_DIR" ] || [ "$(id -u)" -eq 0 ]; then
    mkdir -p "$TARGET_DIR"
    cp -f "$BUILD_DIR/seeker" "$TARGET_DIR/seeker"
    chmod 755 "$TARGET_DIR/seeker"
    ln -sf "$TARGET_DIR/seeker" "$TARGET_DIR/seek"
else
    if command -v sudo >/dev/null 2>&1; then
        echo -e "${YELLOW}Elevated permissions required for $TARGET_DIR. Using sudo...${NC}"
        sudo mkdir -p "$TARGET_DIR"
        sudo cp -f "$BUILD_DIR/seeker" "$TARGET_DIR/seeker"
        sudo chmod 755 "$TARGET_DIR/seeker"
        sudo ln -sf "$TARGET_DIR/seeker" "$TARGET_DIR/seek"
    else
        echo -e "${YELLOW}Cannot write to $TARGET_DIR without sudo. Installing to $HOME/.local/bin instead...${NC}"
        TARGET_DIR="$HOME/.local/bin"
        mkdir -p "$TARGET_DIR"
        cp -f "$BUILD_DIR/seeker" "$TARGET_DIR/seeker"
        chmod 755 "$TARGET_DIR/seeker"
        ln -sf "$TARGET_DIR/seeker" "$TARGET_DIR/seek"
    fi
fi

echo ""
echo -e "${BOLD}${GREEN}==> 'seeker' and 'seek' successfully installed to $TARGET_DIR!${NC}"

# Check PATH
if [[ ":$PATH:" != *":$TARGET_DIR:"* ]]; then
    echo -e "${YELLOW}Note: '$TARGET_DIR' is not in your current PATH.${NC}"
    echo -e "Add it to your ~/.bashrc or ~/.zshrc:"
    echo -e "    ${BOLD}export PATH=\"$TARGET_DIR:\$PATH\"${NC}"
fi

echo -e "Run ${BOLD}seek --help${NC} or ${BOLD}seeker --help${NC} to get started."
