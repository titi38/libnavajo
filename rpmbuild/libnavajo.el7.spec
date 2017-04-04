
# rpmbuild -bb --clean --target x86_64 rpmbuild/libnavajo.el7.spec

%define major 1
%define updatemajor 0

%define libname libnavajo
%define develname libnavajo-devel

Summary:	Webserver and custom web interfaces integration for C++ applications
Name:		libnavajo
Version:	1.5.0
Release:	1
License:	LGPLv3
Group:		System/Libraries
URL:		  http://www.libnavajo.org
BuildRequires:	zlib-devel openssl-devel
BuildRequires:	libtool  openssl zlib automake cmake
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot
Source: libnavajo-%{version}.tar.bz2

%description
libnavajo is an implementation of a complete HTTP(S) server, complete, fast
and lightweight.

#%package -n	%{libname}
Summary:	Shared library part of libnavajo
Group:		System/Libraries

%description -n	%{libname}
libnavajo is an implementation of a complete HTTP(S) server, complete, fast
and lightweight.

libnavajo makes it easy to run an HTTP server into your own application, 
implement dynamic pages (without needing any middleware !), and include all
the web pages you want (local or compiled into your project, thanks to our
navajo precompiler !)


%package -n	%{develname}
Summary:	Headers and libraries needed for libnavajo development
Group:		Development/C
Requires:	%{libname} = %{version}
Provides:	lib%{name}-devel = %{version}

%description -n	%{develname}
C++ API to develop easily web interfaces in your C++ applications

libnavajo is an implementation of a complete HTTP(S) server, complete, fast
and lightweight.

libnavajo makes it easy to run an HTTP server into your own application, 
implement dynamic pages (without needing any middleware !), and include all
the web pages you want (local or compiled into your project, thanks to our
navajo precompiler !)

Implementation is HTTP 1.1 compliant with SSL, X509 and HTTP authentification,
Zlib compression, Fully configurable pthreads pool, Support for IPv4 and IPv6, 
Java style session support, multiple container types (dynamic, local, or 
precompiled in code)

%prep
%setup -q -n %{libname}

%build
cmake -DCMAKE_INSTALL_PREFIX:PATH=$RPM_BUILD_ROOT/usr
make clean
make
doxygen

%install
rm -rf %{buildroot}
make install
%ifarch x86_64 amd64 ia32e
mv $RPM_BUILD_ROOT/usr/lib $RPM_BUILD_ROOT/usr/lib64
%endif

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc LICENSE
%{_bindir}/navajoPrecompiler
%{_mandir}/man3/*


%files -n %{libname}
%defattr(-,root,root)
%{_libdir}/libnavajo.so
%{_libdir}/libnavajo.so.%{major}*

%files -n %{develname}
%defattr(-,root,root)
%{_libdir}/lib*.so
%{_libdir}/lib*.a
%{_includedir}/*
%{_bindir}/navajoPrecompiler

%changelog
* Tue Apr 4 2017 Thierry Descombes <thierry.descombes@gmail.com> 1.5.0-el7
- new packaged release 1.5.0
* Fri Apr 15 2016 Thierry Descombes <thierry.descombes@gmail.com> 1.2.0-el7
- new packaged release 1.2.0
* Thu Apr 16 2015 Thierry Descombes <thierry.descombes@gmail.com> 1.0.0-el7
- first packaged release 1.0.0


