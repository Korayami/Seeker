#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
cd "$ROOT_DIR"

VERSION="1.0.0"
ARCH="$(uname -m)"
case "$ARCH" in
    x86_64) DEB_ARCH="amd64" ;;
    aarch64|arm64) DEB_ARCH="arm64" ;;
    armv7l) DEB_ARCH="armhf" ;;
    *) DEB_ARCH="$ARCH" ;;
esac

PKG_NAME="seeker"
DIST_DIR="$ROOT_DIR/dist"
PKG_DIR="$DIST_DIR/${PKG_NAME}_${VERSION}_${DEB_ARCH}"

echo "==> Preparing Debian package directory for ${DEB_ARCH}..."
rm -rf "$PKG_DIR"
mkdir -p "$PKG_DIR/DEBIAN"
mkdir -p "$PKG_DIR/usr/bin"
mkdir -p "$PKG_DIR/usr/share/doc/$PKG_NAME"

echo "==> Compiling seeker binary..."
CC="${CC:-$(command -v gcc || command -v clang || echo cc)}"
$CC -O3 -Wall -Wextra -pedantic "$ROOT_DIR/seek.c" -o "$PKG_DIR/usr/bin/seeker"
chmod 755 "$PKG_DIR/usr/bin/seeker"
ln -sf seeker "$PKG_DIR/usr/bin/seek"

echo "==> Creating DEBIAN/control file..."
cat <<EOF > "$PKG_DIR/DEBIAN/control"
Package: ${PKG_NAME}
Version: ${VERSION}
Section: utils
Priority: optional
Architecture: ${DEB_ARCH}
Maintainer: Killian-Alexander Beck
Description: Fast CLI file and content search tool
 Seek provides high-performance file name and file content searching
 with flexible glob patterns and path filtering.
EOF

chmod 644 "$PKG_DIR/DEBIAN/control"

DEB_FILE="$DIST_DIR/${PKG_NAME}_${VERSION}_${DEB_ARCH}.deb"

if command -v dpkg-deb >/dev/null 2>&1; then
    echo "==> Building .deb package with dpkg-deb..."
    dpkg-deb --build "$PKG_DIR" "$DEB_FILE"
else
    echo "==> Building standard .deb package..."
    python3 -c "
import tarfile, os, io

pkg_dir = '$PKG_DIR'
deb_path = '$DEB_FILE'

# 1. debian-binary
deb_binary = b'2.0\n'

# 2. control.tar.gz
control_buf = io.BytesIO()
with tarfile.open(fileobj=control_buf, mode='w:gz') as tar:
    tar.add(os.path.join(pkg_dir, 'DEBIAN', 'control'), arcname='./control')
control_data = control_buf.getvalue()

# 3. data.tar.gz
data_buf = io.BytesIO()
with tarfile.open(fileobj=data_buf, mode='w:gz') as tar:
    tar.add(os.path.join(pkg_dir, 'usr'), arcname='./usr')
data_data = data_buf.getvalue()

def ar_member(name, content):
    header = f'{name:<16}{0:<12}{0:<6}{0:<6}{100644:<8o}{len(content):<10}\`\n'.encode('ascii')
    padding = b'\n' if len(content) % 2 != 0 else b''
    return header + content + padding

with open(deb_path, 'wb') as f:
    f.write(b'!<arch>\n')
    f.write(ar_member('debian-binary', deb_binary))
    f.write(ar_member('control.tar.gz', control_data))
    f.write(ar_member('data.tar.gz', data_data))
"
fi

echo ""
echo "=========================================================="
echo " Successfully built: $DEB_FILE"
echo " Install with: sudo apt install ./dist/${PKG_NAME}_${VERSION}_${DEB_ARCH}.deb"
echo "=========================================================="
