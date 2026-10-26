#!/usr/bin/env bash
set -euo pipefail

# This script configures a local APT repository on Debian / Raspberry Pi OS
# so that you can install seeker using: 'sudo apt install seeker'

REPO_DIR="/var/local/seeker-apt"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "==> 1. Building seeker Debian package..."
"$ROOT_DIR/packaging/debian/build-deb.sh"

echo "==> 2. Setting up repository directory at $REPO_DIR..."
sudo mkdir -p "$REPO_DIR"
sudo cp "$ROOT_DIR"/dist/seeker_*.deb "$REPO_DIR/"

# Ensure dpkg-scanpackages is available
if ! command -v dpkg-scanpackages >/dev/null 2>&1; then
    echo "==> Installing dpkg-dev for package indexing..."
    sudo apt-get update && sudo apt-get install -y dpkg-dev
fi

echo "==> 3. Generating repository metadata..."
cd "$REPO_DIR"
dpkg-scanpackages . /dev/null | gzip -9c | sudo tee "$REPO_DIR/Packages.gz" > /dev/null

echo "==> 4. Registering repository in /etc/apt/sources.list.d/seeker-local.list..."
echo "deb [trusted=yes] file:$REPO_DIR ./" | sudo tee /etc/apt/sources.list.d/seeker-local.list > /dev/null

echo "==> 5. Updating apt index..."
sudo apt-get update

echo ""
echo "=========================================================="
echo " Success! You can now install seeker by running:"
echo "     sudo apt install seeker"
echo "=========================================================="
