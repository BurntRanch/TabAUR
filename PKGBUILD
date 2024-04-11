pkgname="TabAUR-git"
pkgver=0.5.8
pkgrel=1
pkgdesc="A lightweight AUR helper, designed to be simple but powerful."
arch=('x86_64' 'aarch64')
url="https://example.com/"
license=('GPL3')
depends=('pacman' 'tar')
optdepends=(
  'sudo: privilege elevation'
  'doas: privilege elevation'
)
#sha256sums=(SKIP)

prepare() {
    git submodule init
    git submodule update --init --recursive
}

build() {
    cd "$srcdir/.." && make -j $(nproc)
}

package() {
    cd "$srcdir/.." && install -Dm755 "taur" "${pkgdir}/usr/local/bin/taur"
    #install -Dm644 "LICENSE" "${pkgdir}/usr/share/licenses/${pkgname}/LICENSE"
}
