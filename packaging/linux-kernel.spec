Name: linux-kernel
Summary: The Linux Kernel
Version: 3.8.3
Release: 1
License: GPL
Group: System Environment/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0:   %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

%description
The Linux Kernel, the operating system core itself

%package headers
Summary: Header files for the Linux kernel for use by glibc
Group: Development/System
Obsoletes: kernel-headers
Provides: kernel-headers = %{version}
%description headers
Kernel-headers includes the C header files that specify the interface
between the Linux kernel and userspace libraries and programs.  The
header files define structures and constants that are needed for
building most standard programs and are also needed for rebuilding the
glibc package.

%prep
%setup -q

%build

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT
mkdir $RPM_BUILD_ROOT/usr
make ARCH=arm INSTALL_HDR_PATH=$RPM_BUILD_ROOT/usr headers_install

%clean
rm -rf $RPM_BUILD_ROOT

%files headers
%defattr (-, root, root)
/usr/include
