%global major 1

Name:           libnavajo
Version:        1.8.0
Release:        1%{?dist}
Summary:        C++ framework for Web and RESTful API development

License:        LGPL-3.0-or-later
URL:            https://github.com/titi38/libnavajo
Source0:        %{name}-%{version}.tar.bz2

BuildRequires:  gcc
BuildRequires:  gcc-c++
BuildRequires:  make
BuildRequires:  cmake
BuildRequires:  zlib-devel
BuildRequires:  openssl-devel

%description
libnavajo is a lightweight C++ HTTP(S) server framework for embedding web
interfaces and RESTful APIs into C++ applications.

%package devel
Summary:        Headers and development files for %{name}
Requires:       %{name}%{?_isa} = %{version}-%{release}

%description devel
Development headers and libraries for building applications with libnavajo.

%prep
%autosetup -n %{name}

%build
%cmake
%cmake_build

%install
%cmake_install

%files
%license LICENSE
%{_bindir}/navajoPrecompiler
%{_libdir}/libnavajo.so.%{major}
%{_libdir}/libnavajo.so.%{major}.*
%{_mandir}/man3/*

%files devel
%{_includedir}/libnavajo/
%{_includedir}/MPFDParser/
%{_libdir}/libnavajo.so
%{_libdir}/libnavajo.a

%changelog
* Tue May 12 2026 Thierry Descombes <thierry.descombes@gmail.com> - 1.8.0-1
- Add RFC6455 WebSocket compliance and modern browser compatibility
- Improve HTTP/WebSocket safety, repositories, multipart handling, and logging
