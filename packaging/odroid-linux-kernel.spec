%define config_name tizen_odroid_defconfig
%define buildarch arm
%define target_board odroidxu3
%define variant %{buildarch}-%{target_board}

Name: odroid-linux-kernel
Summary: The Linux Kernel for ODROID XU3
Version: 4.14.85
Release: 0
License: GPL-2.0
ExclusiveArch: %{arm}
Group: System/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0:   linux-kernel-%{version}.tar.xz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

%define fullVersion %{version}-%{variant}

BuildRequires: module-init-tools
BuildRequires: u-boot-tools
BuildRequires: bc

%description
The Linux Kernel, the operating system core itself

%package -n %{variant}-linux-kernel
Summary: Tizen kernel for %{target_board}
Group: System/Kernel
Provides: %{variant}-odroid-kernel-profile_common = %{version}-%{release}
Provides: %{variant}-odroid-kernel-profile_mobile = %{version}-%{release}
Provides: %{variant}-odroid-kernel-profile_tv = %{version}-%{release}
Provides: %{variant}-odroid-kernel-profile_ivi = %{version}-%{release}
Provides: %{variant}-kernel-uname-r = %{fullVersion}
Provides: linux-kernel = %{version}-%{release}

%description -n %{variant}-linux-kernel
This package contains the Linux kernel for Tizen (common/mobile/tv/ivi profile, arch %{buildarch}, target board %{target_board})

%package -n %{variant}-linux-kernel-modules
Summary: Kernel modules for %{target_board}
Group: System/Kernel
Provides: %{variant}-kernel-modules = %{fullVersion}
Provides: %{variant}-kernel-modules-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel-modules
Kernel-modules includes the loadable kernel modules(.ko files) for %{target_board}

%package -n %{variant}-linux-kernel-devel
License: GPL-2.0
Summary: Linux support kernel map and etc for other packages
Group: System/Kernel
Provides: %{variant}-kernel-devel = %{fullVersion}
Provides: %{variant}-kernel-devel-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel-devel
This package provides kernel map and etc information for odroid kernel.

%prep
%setup -q -n linux-kernel-%{version}

%build
%{?asan:/usr/bin/gcc-unforce-options}
%{?ubsan:/usr/bin/gcc-unforce-options}

# Make sure EXTRAVERSION says what we want it to say
sed -i "s/^EXTRAVERSION.*/EXTRAVERSION = -%{release}-%{variant}/" Makefile

# 1. Compile sources
make %{config_name}
make %{?_smp_mflags}

# 2. Build zImage
make zImage %{?_smp_mflags}
make dtbs %{?_smp_mflags}

# 3. Build modules
make modules %{?_smp_mflags}

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 1. Destynation directories
mkdir -p %{buildroot}/boot/
mkdir -p %{buildroot}/lib/modules/%{fullVersion}

# 2. Install zImage, System.map, ...
install -m 755 arch/arm/boot/zImage %{buildroot}/boot/
install -m 644 arch/arm/boot/dts/*odroid*.dtb %{buildroot}/boot/

install -m 644 System.map %{buildroot}/boot/System.map-%{fullVersion}
install -m 644 .config %{buildroot}/boot/config-%{fullVersion}

# 3. Install modules
make INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=%{buildroot} modules_install KERNELRELEASE=%{fullVersion}

rm -rf %{buildroot}/boot/vmlinux*
rm -rf %{buildroot}/System.map*
rm -rf %{buildroot}/vmlinux*

# 7. Update file permisions
find %{buildroot}/lib/modules/ -name "*.ko" -type f -print0 | xargs -0 chmod 755

# 8. Create symbolic links
rm -f %{buildroot}/lib/modules/%{fullVersion}/build
rm -f %{buildroot}/lib/modules/%{fullVersion}/source

# 9-1. remove unnecessary files to prepare for devel package
rm -f tools/mkimage*
find %{_builddir}/linux-kernel-%{version} -name "*\.HEX" -type f -delete
find %{_builddir}/linux-kernel-%{version} -name "vdso.so.raw" -type f -delete
find %{_builddir}/linux-kernel-%{version} -name ".tmp_vmlinux*" -delete
find %{_builddir}/linux-kernel-%{version} -name ".gitignore" -delete
find %{_builddir}/linux-kernel-%{version} -name "\.*dtb*tmp" -delete
find %{_builddir}/linux-kernel-%{version} -name "\.*dtb" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.*tmp" -delete
find %{_builddir}/linux-kernel-%{version} -name "vmlinux" -delete
find %{_builddir}/linux-kernel-%{version} -name "zImage" -delete
find %{_builddir}/linux-kernel-%{version} -name "*.cmd" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.ko" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.o" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.S" -delete
find %{_builddir}/linux-kernel-%{version} -name "*\.c" -not -path "%{_builddir}/linux-kernel-%{version}/scripts/*" -delete

# 9-2. copy devel package
mkdir -p %{buildroot}/boot/kernel/devel
cp -r %{_builddir}/linux-kernel-%{version} %{buildroot}/boot/kernel/devel/kernel-devel-%{variant}

%clean
rm -rf %{buildroot}

%files -n %{variant}-linux-kernel-modules
/lib/modules/*

%files -n %{variant}-linux-kernel-devel
/boot/kernel/devel/*

%files -n %{variant}-linux-kernel
%license COPYING
/boot/zImage
/boot/*.dtb
/boot/System.map*
/boot/config*
