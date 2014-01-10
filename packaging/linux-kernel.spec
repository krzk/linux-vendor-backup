%define config_name tizen_defconfig
%define abiver 1
%define build_id %{config_name}.%{abiver}
%define defaultDtb exynos4412-m0.dtb
%define buildarch arm

Name: linux-kernel
Summary: The Linux Kernel
Version: 3.10.19
Release: 1
License: GPL-2.0
Group: System Environment/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0:   %{name}-%{version}-%{build_id}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

BuildRequires: linux-glibc-devel
BuildRequires: u-boot-tools
BuildRequires: bc

%description
The Linux Kernel, the operating system core itself

%package user-headers
Summary: Header files for the Linux kernel for use by glibc
Group: Development/System
Obsoletes: kernel-headers
Provides: kernel-headers = %{version}

%description user-headers
Kernel-headers includes the C header files that specify the interface
between the Linux kernel and userspace libraries and programs.  The
header files define structures and constants that are needed for
building most standard programs and are also needed for rebuilding the
glibc package.

%package devel
Summary: Prebuilt linux kernel for out-of-tree modules
Group: Development/System

%description devel
Prebuilt linux kernel for out-of-tree modules.

%package image
Summary: Linux kernel image
Group: Development/System

%description image
Linux kernel uImage

%prep
%setup -q

%build
# 1. Compile sources
make EXTRAVERSION="-%{build_id}" %{config_name}
make EXTRAVERSION="-%{build_id}" %{?_smp_mflags}

# 2. Build uImage
make EXTRAVERSION="-%{build_id}" zImage %{?_smp_mflags}
make EXTRAVERSION="-%{build_id}" dtbs %{?_smp_mflags}
cat arch/arm/boot/zImage arch/arm/boot/dts/%{defaultDtb}  > bImage
mkimage -A arm -C none -O linux -a 40008000 -e 40008000 -n 'Linux 3.10 Tizen kernel' -d bImage uImage

# 3. Build modules
#make EXTRAVERSION="-%{build_id}" modules %{?_smp_mflags}

# 4. Create tar repo for build directory
tar cpsf linux-kernel-build-%{version}-%{build_id}.tar .

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 1. Destynation directories
mkdir -p %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}
mkdir -p %{buildroot}/lib/modules/%{version}-%{build_id}
mkdir -p %{buildroot}/boot/

# 2. Install uImage
install uImage %{buildroot}/boot/

# 3. Install modules
#make INSTALL_MOD_PATH=%{buildroot} modules_install

# 4. Install kernel headers
make INSTALL_PATH=%{buildroot} INSTALL_MOD_PATH=%{buildroot} INSTALL_HDR_PATH=%{buildroot}/usr headers_install

# 5. Restore source and build irectory
tar -xf linux-kernel-build-%{version}-%{build_id}.tar -C %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}
mv %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}/arch/%{buildarch} .
mv %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}/arch/Kconfig .
rm -rf %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}/arch/*
mv %{buildarch} %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}/arch/
mv Kconfig      %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}/arch/

# 6. Remove files
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name ".tmp_vmlinux1" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name ".tmp_vmlinux2" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "vmlinux" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "uImage" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "zImage" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*.cmd" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.o" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "\.*dtb*tmp" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.*tmp" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.S" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.ko" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.c" -exec rm -f {} \;

find %{buildroot}/usr -name "..install.cmd" -exec rm -f {} \;
find %{buildroot}/usr/include -name "\.\.install.cmd"  -exec rm -f {} \;
find %{buildroot}/usr/include -name "\.install"  -exec rm -f {} \;

rm -f  %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}/source
rm -rf %{buildroot}/System.map*
rm -rf %{buildroot}/vmlinux*
rm -rf %{buildroot}/boot/System.map*
rm -rf %{buildroot}/boot/vmlinux*

# 7. Create symbolic links
rm -f %{buildroot}/usr/src/linux-kernel-build-current
rm -f %{buildroot}/lib/modules/%{version}-%{build_id}/build
rm -f %{buildroot}/lib/modules/%{version}-%{build_id}/source
ln -sf /usr/src/linux-kernel-build-%{version}-%{build_id} %{buildroot}/lib/modules/%{version}-%{build_id}/build

%clean
rm -rf %{buildroot}

%files user-headers
%defattr (-, root, root)
/usr/include

%files devel
%defattr (-, root, root)
/usr/src/linux-kernel-build-%{version}-%{build_id}
#/lib/modules/%{version}-%{build_id}/kernel
/lib/modules/%{version}-%{build_id}/build
#/lib/modules/%{version}-%{build_id}/modules.*

%files image
/boot/uImage
#/lib/modules/%{version}-%{build_id}/kernel
#/lib/modules/%{version}-%{build_id}/modules.*
