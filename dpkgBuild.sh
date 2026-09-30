#!/bin/sh

set -e

LIB_VERSION=1.9-2
ARCH_NAME="$(dpkg --print-architecture)"

export DPKG_BUILD_ROOT=debianBuild
BUILD_DIR=cmake-build

rm -rf "$BUILD_DIR" "$DPKG_BUILD_ROOT"
mkdir -p "$DPKG_BUILD_ROOT"

cmake -S . -B "$BUILD_DIR" \
    -DCMAKE_INSTALL_PREFIX:PATH="$PWD/$DPKG_BUILD_ROOT/usr"

cmake --build "$BUILD_DIR" -j"$(nproc)"


if command -v doxygen >/dev/null 2>&1; then
    doxygen
else
    echo "WARNING: doxygen not found, skipping documentation generation."
fi

cmake --install "$BUILD_DIR"

find "$DPKG_BUILD_ROOT" -type f | sed "s/$DPKG_BUILD_ROOT.\(.*\)\/\(.*\)/\2\t\t\1/" > install
find "$DPKG_BUILD_ROOT" -type d | sed "s/$DPKG_BUILD_ROOT\///" | grep -v "$DPKG_BUILD_ROOT" > dirs

mkdir -p "$DPKG_BUILD_ROOT/DEBIAN"
mv install "$DPKG_BUILD_ROOT/DEBIAN"
mv dirs "$DPKG_BUILD_ROOT/DEBIAN"

cat > "$DPKG_BUILD_ROOT/DEBIAN/control" << EOF_CONTROL
Package: libnavajo
Section: Developpement
Priority: optional
Maintainer: Thierry DESCOMBES <thierry.descombes@gmail.com>
Architecture: $ARCH_NAME
Depends: openssl, zlib1g-dev
Version: 1.8
Description: an implementation of a complete HTTP(S) server, complete, fast and lightweight.
EOF_CONTROL

cat > "$DPKG_BUILD_ROOT/DEBIAN/rules" << 'EOF_RULES'
#!/usr/bin/make -f
# -*- makefile -*-
# Sample debian/rules that uses debhelper.

clean:
	dh_testdir
	dh_testroot
	dh_clean

install: build
	dh_testdir
	dh_testroot
	dh_clean -k
	dh_installdirs
	dh_installdocs
	dh_install

binary-arch: build install
	dh_testdir
	dh_testroot
	dh_link
	dh_compress
	dh_fixperms
	dh_installdeb
	dh_gencontrol
	dh_md5sums
	dh_builddeb

binary: binary-indep binary-arch
.PHONY: build clean binary-indep binary-arch binary install configure
EOF_RULES

cat > "$DPKG_BUILD_ROOT/DEBIAN/postinst" << 'EOF_POSTINST'
#!/bin/sh
/sbin/ldconfig
EOF_POSTINST

chmod 755 "$DPKG_BUILD_ROOT/DEBIAN/postinst"

dpkg -b "$DPKG_BUILD_ROOT" "libnavajo-$LIB_VERSION.$ARCH_NAME.deb"
