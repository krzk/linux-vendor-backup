%define config_name tizen_tm2_defconfig
%define buildarch arm64
%define target_board tm2
%define variant %{buildarch}-%{target_board}

Name: %{target_board}-linux-kernel
Summary: The Linux Kernel for TM2/TM2E board
Version: 4.1
Release: 0
License: GPL-2.0
ExclusiveArch: aarch64
Group: System/Kernel
Vendor: The Linux Community
URL: https://www.kernel.org
Source0:   linux-kernel-%{version}.tar.xz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

%define fullVersion %{version}-%{variant}
%define __spec_install_post /usr/lib/rpm/brp-compress || :
%define debug_package %{nil}

BuildRequires: bc
BuildRequires: module-init-tools
BuildRequires: u-boot-tools >= 2016.03

%if "%{?profile}" != "mobile"
ExcludeArch: aarch64
%endif

%description
The Linux Kernel, the operating system core itself

%package -n %{variant}-linux-kernel
License: GPL-2.0
Summary: Tizen kernel for %{target_board}
Group: System/Kernel
Provides: %{variant}-kernel-profile-%{profile} = %{version}-%{release}
Provides: %{variant}-kernel-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel
This package contains the Linux kernel for Tizen (%{profile} profile, arch %{buildarch}, target board %{target_board})

%package -n %{variant}-linux-kernel-modules
Summary: Kernel modules for %{target_board}
Group: System/Kernel
Provides: %{variant}-kernel-modules = %{fullVersion}
Provides: %{variant}-kernel-modules-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel-modules
Kernel-modules includes the loadable kernel modules(.ko files) for %{target_board}

%package -n %{variant}-linux-kernel-debuginfo
License: GPL-2.0
Summary: Linux support debug symbol
Group: System/Kernel

%description -n %{variant}-linux-kernel-debuginfo
This package provides the %{target_board} linux kernel's debugging files.

%package -n %{variant}-linux-kernel-devel
License: GPL-2.0
Summary: Linux support kernel map and etc for other packages
Group: System/Kernel
Provides: kernel-devel-tizen-dev
Provides: %{variant}-kernel-devel = %{fullVersion}
Provides: %{variant}-kernel-devel-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel-devel
This package provides kernel map and etc information.

%package -n %{variant}-linux-kernel-headers
License: GPL-2.0
Summary: Linux support headers for userspace development
Group: System/Kernel
Provides: kernel-headers-tizen-dev

%description -n %{variant}-linux-kernel-headers
This package provides userspaces headers from the Linux kernel. These
headers are used by the installed headers for GNU glibc and other system
 libraries.

%prep
%setup -q -n linux-kernel-%{version}

%build
# Make sure EXTRAVERSION says what we want it to say
sed -i "s/^EXTRAVERSION.*/EXTRAVERSION = -%{variant}/" Makefile

# 1-1. extract uapi headers
mkdir -p uapi-headers/usr
make mrproper
make headers_check
make headers_install INSTALL_HDR_PATH=uapi-headers/usr

# 1-2. set config file
make %{config_name}

# 1-3. Build Image/Image.gz
make %{?_smp_mflags}

# 1-4. Build dtbs
make dtbs %{?_smp_mflags}

# 1-5. Build u-boot itb image
mkimage -f arch/arm64/boot/tizen-tm2.its kernel.img

# 1-6. Build modules
make modules %{?_smp_mflags}

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 2-1. Destination directories
mkdir -p %{_builddir}/boot
mkdir -p %{_builddir}/lib/modules

# 2-2. Install kernel.img
install -m 644 kernel.img %{_builddir}/boot/

# 2-3. Install Image.gz, dtbs, System.map, ...
install -m 644 arch/%{buildarch}/boot/Image.gz %{_builddir}/boot/
install -m 644 arch/%{buildarch}/boot/dts/exynos/*.dtb %{_builddir}/boot/
install -m 644 System.map %{_builddir}/boot/
install -m 644 .config %{_builddir}/boot/config-%{fullVersion}
install -m 644 vmlinux %{_builddir}/boot/
install -m 644 COPYING %{_builddir}/boot/

# 2-4. Install modules
make INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=%{_builddir} modules_install

# 2-5. Install uapi headers
find uapi-headers/usr/include -name ".install" | xargs rm -f
find uapi-headers/usr/include -name "..install.cmd" | xargs rm -f
rm -f uapi-headers/usr/include/asm*/atomic.h
rm -f uapi-headers/usr/include/asm*/io.h
mv uapi-headers/usr %{_builddir}/

# 3-1. remove unnecessary files to prepare for devel package
rm -rf arch/%{buildarch}/boot/vmlinux*
rm -rf System.map*
rm -rf vmlinux*
rm -rf kernel.img
rm -rf uapi-headers
rm -f tools/mkimage*
find %{_builddir}/linux-kernel-%{version} -name ".tmp_vmlinux*" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name ".gitignore" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "\.*dtb*tmp" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "\.*dtb" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "*\.*tmp" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "vmlinux" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "Image" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "Image.gz" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "*.cmd" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "*\.ko" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "*\.o" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "*\.S" -exec rm -f {} \;
find %{_builddir}/linux-kernel-%{version} -name "*\.c" -not -path "%{_builddir}/linux-kernel-%{version}/scripts/*" -exec rm -f {} \;

# 3-2. move files for devel package
cd %{_builddir}
mv linux-kernel-%{version} kernel-devel-%{variant}
mkdir -p linux-kernel-%{version}

# 4. Move files for each package
mkdir -p %{buildroot}/boot/kernel/devel
mv %{_builddir}/boot/COPYING %{buildroot}/
mv %{_builddir}/boot/* %{buildroot}/boot/
rm -rf %{_builddir}/boot
mv %{_builddir}/lib %{buildroot}/
mv %{_builddir}/usr %{buildroot}/
mv %{_builddir}/kernel-devel-%{variant} %{buildroot}/boot/kernel/devel/

%clean
rm -rf %{buildroot}
rm -rf %{_builddir}/COPYING
rm -rf %{_builddir}/boot
rm -rf %{_builddir}/lib
rm -rf %{_builddir}/usr
rm -rf %{_builddir}/kernel-devel-%{variant}

%files -n %{variant}-linux-kernel
%license /COPYING
/boot/kernel.img

%files -n %{variant}-linux-kernel-modules
/lib/modules/

%files -n %{variant}-linux-kernel-devel
/boot/kernel/devel/*

%files -n %{variant}-linux-kernel-headers
/usr/include/*

%files -n %{variant}-linux-kernel-debuginfo
/boot/Image.gz
/boot/*.dtb
/boot/System.map*
/boot/config*
/boot/vmlinux*
