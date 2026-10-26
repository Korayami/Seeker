# Maintainer: Killian-Alexander Beck
pkgname=seeker
pkgver=1.0.0
pkgrel=1
pkgdesc="Fast CLI file and content search tool"
arch=('x86_64' 'aarch64' 'armv7h')
license=('MIT')
depends=('glibc')
makedepends=('gcc' 'make')
source=("seek.c"
        "seekui.c"
        "Makefile")
sha256sums=('SKIP'
            'SKIP'
            'SKIP')

build() {
    cd "$srcdir"
    make
}

package() {
    cd "$srcdir"
    make DESTDIR="$pkgdir" PREFIX="/usr" install
}
