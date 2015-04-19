
# rpmbuild -bb --clean --target x86_64 rpm/libnavajo.el6.spec

%define major 1
%define updatemajor 0

%define libname libnavajo
%define develname libnavajo-devel

Summary:	Webserver and custom web interfaces integration for C++ applications
Name:		navajo
Version:	1.0.0
Release:	1
License:	LGPLv3
Group:		System/Libraries
URL:		  http://www.libnavajo.org
BuildRequires:	zlib-devel openssl-devel pam-devel
BuildRequires:	libtool  openssl zlib pam
BuildRoot:	%{_tmppath}/%{name}-%{version}-%{release}-buildroot
Source: libnavajo-%{version}.tar.bz2

%description
libnavajo is an implementation of a complete HTTP(S) server, complete, fast
and lightweight.

%package -n	%{libname}
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
make clean
make

%install
rm -rf %{buildroot}
make install PREFIX=$RPM_BUILD_ROOT/usr

%clean
rm -rf %{buildroot}

%files
%defattr(-,root,root)
%doc LICENSE
%{_bindir}/navajoPrecompiler
#%{_mandir}/man3/libnavajo.3*


%files -n %{libname}
%defattr(-,root,root)
%{_libdir}/libnavajo.so
%{_libdir}/libnavajo.so.%{major}*

%files -n %{develname}
%defattr(-,root,root)
%{_libdir}/lib*.so
%{_libdir}/lib*.a
%{_includedir}/*


%changelog
* Thu Apr 16 2015 Thierry Descombes <thierry.descombes@gmail.com> 1.0.0-el6
- first packaged release 1.0.0

