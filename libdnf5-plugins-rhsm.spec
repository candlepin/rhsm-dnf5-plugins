Name:           libdnf5-plugins-rhsm
Version:        0.1.1
Epoch:          1
Release:        %autorelease
Summary:        Libdnf5 plugins for Red Hat Subscription Management
License:        GPL-3.0-only
URL:            https://github.com/candlepin/rhsm-dnf5-plugins
Source0:        %{name}-%{version}.tar.gz

%bcond_with     clang
%bcond_with     tests

%if %{with clang}
BuildRequires:  clang
%else
BuildRequires:  gcc-c++ >= 10.1
%endif
BuildRequires:  cmake >= 3.21
BuildRequires:  pkgconfig(libcrypto)
BuildRequires:  libdnf5-devel
BuildRequires:  jsoncpp-devel
%if %{with tests}
BuildRequires:  gtest-devel
%endif

%description
This package provides libdnf5 plugins to interact with repositories
and subscriptions from the Red Hat entitlement platform; contains
rhsm and product-id plugins.

%files

%{_libdir}/libdnf5/plugins/productid.*
%config(noreplace) %{_sysconfdir}/dnf/libdnf5-plugins/productid.conf

%{_libdir}/libdnf5/plugins/rhsm.*
%config(noreplace) %{_sysconfdir}/dnf/libdnf5-plugins/rhsm.conf

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
