%global pybasever 2.7
%global lzss_python_installs /opt/lzss/python
%global __python %{lzss_python_installs}/python-%{pybasever}/bin/python%{pybasever}

%define _unpackaged_files_terminate_build 0

# The original name from upstream sources.
%global original_name lzss


%define base_version 0.2
%define version %base_version
%define release 1

%if 0%{?__build_number:1}
    %define version %{base_version}.%{__build_number}
%endif

Summary: lzss
Name: lzss-python-%{original_name}2.7
Version: %{version}
Release: %{release}%{?dist}
Source0: %{original_name}-%{version}.tar.gz
License: PUBLIC DOMAIN
Group: Development/Libraries
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-buildroot
Prefix: %{_prefix}
BuildArch: x86_64
Vendor: Haruhiko Okumura
Url: https://github.com/halm90/lzss

BuildRequires: lzss-python%{pybasever}
Requires: lzss-python%{pybasever}

AutoReqProv: no

%description
lzss

%prep
%setup -n %{original_name}-%{version}

%build
export LD_RUN_PATH=/opt/lzss/python/python-2.7/lib
%{__python} setup.py build

%install
%{__python} setup.py install -O1 --root=$RPM_BUILD_ROOT --record=INSTALLED_FILES

%clean
rm -rf $RPM_BUILD_ROOT

%files -f INSTALLED_FILES
%defattr(-,root,root)
