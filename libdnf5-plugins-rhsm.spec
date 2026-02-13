Name:           libdnf5-plugins-rhsm
Version:        0.1
Release:        %autorelease
Summary:        Libdnf5 plugin for product certificates management
License:        GPL-3.0-only
URL:            https://github.com/candlepin/rhsm-dnf5-plugins
Source0:        %{name}-%{version}.tar.gz

%bcond_with     clang
%bcond_with     tests

Requires:       libdnf5
%if %{with clang}
BuildRequires:  clang
%else
BuildRequires:  gcc-c++ >= 10.1
%endif
BuildRequires:  cmake >= 3.21
BuildRequires:  pkgconfig(libcrypto)

%description
Libdnf5 plugin for management of product certificates

%files

%{_libdir}/libdnf5/plugins/productid.*
%config(noreplace) %{_sysconfdir}/dnf/libdnf5-plugins/productid.conf

%prep
%autosetup -p1

%build
%cmake
%cmake_build

%check
%if %{with tests}
    %ctest
%endif

%install
%cmake_install

%changelog
%autochangelog
