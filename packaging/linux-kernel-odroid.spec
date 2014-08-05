%define config_name tizen_odroid_defconfig
%define abiver 1
%define build_id %{config_name}.%{abiver}
%define buildarch arm

Name: linux-kernel-odroid
Summary: The Linux Kernel for ODROID U3
Version: 3.10.39
Release: 1
License: GPL-2.0
ExclusiveArch: %{arm}
Group: System Environment/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0:   %{name}-%{version}-%{build_id}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

%define fullVersion %{version}-%{build_id}

BuildRequires: module-init-tools
BuildRequires: u-boot-tools
BuildRequires: bc
BuildRequires: e2fsprogs >= 1.42.11

Provides: kernel-odroid = %{version}-%{release}
Provides: kernel-odroid-uname-r = %{fullVersion}

%description
The Linux Kernel, the operating system core itself

%package modules
Summary: Kernel modules
Group: System/Kernel
Provides: kernel-odroid-modules = %{fullVersion}
Provides: kernel-odroid-modules-uname-r = %{fullVersion}

%description modules
Kernel-modules includes the loadable kernel modules(.ko files).

%prep
%setup -q

%build
# 1. Compile sources
make EXTRAVERSION="-%{build_id}" %{config_name}
make EXTRAVERSION="-%{build_id}" %{?_smp_mflags}

# 2. Build uImage
make EXTRAVERSION="-%{build_id}" uImage %{?_smp_mflags}
make EXTRAVERSION="-%{build_id}" dtbs %{?_smp_mflags}

# 3. Build modules
make EXTRAVERSION="-%{build_id}" modules %{?_smp_mflags}

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 1. Destynation directories
mkdir -p %{buildroot}/boot/
mkdir -p %{buildroot}/lib/modules/%{fullVersion}

# 2. Install uImage, System.map, ...
install -m 755 arch/arm/boot/uImage %{buildroot}/boot/
install -m 644 arch/arm/boot/dts/*.dtb %{buildroot}/boot/

install -m 644 System.map %{buildroot}/boot/System.map-%{fullVersion}
install -m 644 .config %{buildroot}/boot/config-%{fullVersion}

# 3. Install modules
make INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=%{buildroot} modules_install

rm -rf %{buildroot}/boot/vmlinux*
rm -rf %{buildroot}/System.map*
rm -rf %{buildroot}/vmlinux*

# 7. Update file permisions
find %{buildroot}/lib/modules/ -name "*.ko"                                     -type f -exec chmod 755 {} \;

# 8. Create symbolic links
rm -f %{buildroot}/lib/modules/%{fullVersion}/build
rm -f %{buildroot}/lib/modules/%{fullVersion}/source

# 9. Calculate modules.img size
BIN_SIZE=`du -s %{buildroot}/lib/modules | awk {'printf $1;'}`
let BIN_SIZE=${BIN_SIZE}+1024+512

dd if=/dev/zero of=%{buildroot}/boot/modules.img count=${BIN_SIZE} bs=1024
/usr/sbin/mke2fs -t ext4 -F -d %{buildroot}/lib/modules/ %{buildroot}/boot/modules.img

rm -rf %{buildroot}/lib/modules/%{fullVersion}

%clean
rm -rf %{buildroot}

%files modules
/boot/modules.img

%files
%license COPYING
/boot/uImage
/boot/*.dtb
/boot/System.map*
/boot/config*
