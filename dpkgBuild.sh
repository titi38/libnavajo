#!/bin/sh
export DPKG_BUILD_ROOT=debianBuild
rm -rf $DPKG_BUILD_ROOT
mkdir -p $DPKG_BUILD_ROOT
make install PREFIX=$DPKG_BUILD_ROOT/usr

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
Architecture: amd64 
Depends: openssl, zlib1g-dev, libpam-dev
Version: 1.0
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

dpkg -b $DPKG_BUILD_ROOT libnavajo-1.0.amd64.deb

