#!/usr/bin/env bash
set -euo pipefail

# Text formatting
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[0;33m'
RED='\033[0;31m'
BOLD='\033[1m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

echo -e "${BOLD}${BLUE}==> Building seek...${NC}"

# Find available compiler (prefer clang on macOS, fallback to gcc or cc)
CC="${CC:-$(command -v clang || command -v gcc || command -v cc)}"

if [ -z "$CC" ]; then
    echo -e "${RED}Error: No C compiler found (clang, gcc, or cc). Please install Xcode Command Line Tools or GCC.${NC}" >&2
    exit 1
fi

echo -e "Using compiler: ${BOLD}$CC${NC}"
$CC -O3 -Wall -Wextra -pedantic seek.c -o seek
echo -e "${GREEN}✓ Build succeeded.${NC}"

# Determine target directory
# Priority:
# 1. Custom path provided as argument
# 2. --user / -u flag -> ~/.local/bin
# 3. /usr/local/bin (default system-wide)
TARGET_DIR="/usr/local/bin"

if [ "${1:-}" = "--user" ] || [ "${1:-}" = "-u" ]; then
    TARGET_DIR="$HOME/.local/bin"
elif [ -n "${1:-}" ] && [[ "${1:-}" != -* ]]; then
    TARGET_DIR="$1"
fi

mkdir -p "$TARGET_DIR"

echo -e "${BOLD}${BLUE}==> Installing seek to $TARGET_DIR...${NC}"

if [ -w "$TARGET_DIR" ]; then
    cp seek "$TARGET_DIR/seek"
    chmod +x "$TARGET_DIR/seek"
else
    echo -e "${YELLOW}Notice: '$TARGET_DIR' requires root permissions.${NC}"
    if command -v sudo >/dev/null 2>&1; then
        sudo cp seek "$TARGET_DIR/seek"
        sudo chmod +x "$TARGET_DIR/seek"
    else
        echo -e "${RED}Error: Cannot write to $TARGET_DIR and sudo is not available.${NC}" >&2
        echo -e "Try installing to your user directory instead: ${BOLD}./install.sh --user${NC}" >&2
        exit 1
    fi
fi

echo -e "${BOLD}${GREEN}==> 'seek' has been successfully installed to $TARGET_DIR/seek${NC}"

# Check if TARGET_DIR is in PATH
if [[ ":$PATH:" != *":$TARGET_DIR:"* ]]; then
    echo -e "${YELLOW}Note: '$TARGET_DIR' is not in your current PATH.${NC}"
    echo -e "Add it to your shell config (~/.zshrc or ~/.bashrc):"
    echo -e "    ${BOLD}export PATH=\"$TARGET_DIR:\$PATH\"${NC}"
fi

echo -e "You can now run: ${BOLD}seek \"pattern\"${NC}"
