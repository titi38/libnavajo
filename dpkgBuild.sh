#!/bin/sh
LIB_VERSION=1.5
ARCH_NAME=`dpkg --print-architecture`

export DPKG_BUILD_ROOT=debianBuild
rm -rf $DPKG_BUILD_ROOT
mkdir -p $DPKG_BUILD_ROOT

rm -rf CMakeCache.txt  CMakeFiles  cmake_install.cmake
cmake -DCMAKE_INSTALL_PREFIX:PATH=$DPKG_BUILD_ROOT/usr
if [ $? -ne 0 ]; then
  exit 1;
fi
make clean
make
doxygen
make install

find $DPKG_BUILD_ROOT -type f | sed "s/$DPKG_BUILD_ROOT.\(.*\)\/\(.*\)/\2\t\t\1/" > install
find $DPKG_BUILD_ROOT -type d | sed "s/$DPKG_BUILD_ROOT\///" | grep -v "$DPKG_BUILD_ROOT" > dirs

mkdir $DPKG_BUILD_ROOT/DEBIAN
mv install $DPKG_BUILD_ROOT/DEBIAN
mv dirs $DPKG_BUILD_ROOT/DEBIAN

cat > $DPKG_BUILD_ROOT/DEBIAN/control << EOF_CONTROL
Package: libnavajo
Section: Developpement
Priority: optional
Maintainer: Thierry DESCOMBES <thierry.descombes@gmail.com>
Architecture: `echo $ARCH_NAME`
Depends: openssl, zlib1g-dev
Version: 1.5
Description: an implementation of a complete HTTP(S) server, complete, fast and lightweight.
EOF_CONTROL

cat > $DPKG_BUILD_ROOT/DEBIAN/rules << EOF_RULES
#!/usr/bin/make -f
# -*- makefile -*-
# Sample debian/rules that uses debhelper.
# This file was originally written by Joey Hess and Craig Small.
# As a special exception, when this file is copied by dh-make into a
# dh-make output file, you may use that output file without restriction.
# This special exception was added by Craig Small in version 0.37 of dh-make.

# Uncomment this to turn on verbose mode.
#export DH_VERBOSE=1

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

# Build architecture-dependent files here.
binary-arch: build install
	dh_testdir
	dh_testroot
#	dh_installchangelogs 
#	dh_installdocs
#	dh_installexamples
#	dh_install
#	dh_installmenu
#	dh_installdebconf	
#	dh_installlogrotate
#	dh_installemacsen
#	dh_installpam
#	dh_installmime
#	dh_installinit
#	dh_installcron
#	dh_installinfo
#	dh_installman
	dh_link
#	dh_strip
	dh_compress
	dh_fixperms
#	dh_perl
#	dh_python
#	dh_makeshlibs
	dh_installdeb
#	dh_shlibdeps
	dh_gencontrol
	dh_md5sums
	dh_builddeb

binary: binary-indep binary-arch
.PHONY: build clean binary-indep binary-arch binary install configure
EOF_RULES


cat > $DPKG_BUILD_ROOT/DEBIAN/postinst << EOF_POSTINST
#!/bin/sh
/sbin/ldconfig
EOF_POSTINST
chmod 755 $DPKG_BUILD_ROOT/DEBIAN/postinst

dpkg -b $DPKG_BUILD_ROOT libnavajo-$LIB_VERSION.$ARCH_NAME.deb

