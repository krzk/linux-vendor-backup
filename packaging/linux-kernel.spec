%define config_name tizen_defconfig
%define buildarch arm
%define target_board trats2
%define variant %{buildarch}-%{target_board}

Name: linux-kernel
Summary: The Linux Kernel for Samsung Exynos
Version: 3.10.52
Release: 0
License: GPL-2.0
ExclusiveArch: %{arm}
Group: System/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0:   %{name}-%{version}.tar.xz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

%define fullVersion %{version}-%{variant}

BuildRequires: linux-glibc-devel
BuildRequires: module-init-tools
BuildRequires: u-boot-tools
BuildRequires: bc
# The below is required for building perf
BuildRequires: libelf-devel
BuildRequires: flex
BuildRequires: bison
BuildRequires: libdw-devel
BuildRequires: python-devel
BuildRequires: e2fsprogs >= 1.42.11

%description
The Linux Kernel, the operating system core itself

%package -n %{variant}-linux-kernel
Summary: Tizen kernel
Group: System/Kernel
%if %{profile} == "mobile"
Provides: kernel-profile-%{profile} = %{version}-%{release}
Provides: kernel-uname-r = %{fullVersion}
%endif

%description -n %{variant}-linux-kernel
This package contains the Linux kernel for Tizen (%{profile} profile, arch %{buildarch}, target board %{target_board})

%package -n %{variant}-linux-kernel-headers
Summary: Header files for the Linux kernel for use by glibc
Group: Development/System
%if %{profile} == "mobile"
Obsoletes: kernel-headers
Provides: kernel-headers = %{version}-%{release}
%endif

%description -n %{variant}-linux-kernel-headers
Kernel-headers includes the C header files that specify the interface
between the Linux kernel and userspace libraries and programs.  The
header files define structures and constants that are needed for
building most standard programs and are also needed for rebuilding the
glibc package.

%package -n %{variant}-linux-kernel-modules
Summary: Kernel modules for %{target_board}
Group: System/Kernel
%if %{profile} == "mobile"
Provides: kernel-modules = %{fullVersion}
Provides: kernel-modules-uname-r = %{fullVersion}
%endif

%description -n %{variant}-linux-kernel-modules
Kernel-modules includes the loadable kernel modules(.ko files) for %{target_board}

%package -n %{variant}-linux-kernel-devel
Summary: Prebuilt linux kernel for out-of-tree modules
Group: Development/System
%if %{profile} == "mobile"
Provides: kernel-devel = %{fullVersion}
Provides: kernel-devel-uname-r = %{fullVersion}
%endif
Requires: %{name} = %{version}-%{release}

%description -n %{variant}-linux-kernel-devel
Prebuilt linux kernel for out-of-tree modules.

%package -n %{variant}-linux-kernel-perf
Summary: The 'perf' performance counter tool
Group: Development/System
%if %{profile} == "mobile"
Provides: perf = %{fullVersion}
%endif

%description -n %{variant}-linux-kernel-perf
This package provides the "perf" tool that can be used to monitor performance
counter events as well as various kernel internal events.

%prep
%setup -q

%build
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

# 3.1 Build perf
make -s -C tools/lib/traceevent ARCH=%{buildarch} %{?_smp_mflags}
make -s -C tools/perf WERROR=0 ARCH=%{buildarch}

# 4. Create tar repo for build directory
tar cpf linux-kernel-build-%{fullVersion}.tar .

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 1. Destynation directories
mkdir -p %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}
mkdir -p %{buildroot}/boot/
mkdir -p %{buildroot}/lib/modules/%{fullVersion}

# 2. Install zImage, System.map, ...
install -m 755 arch/arm/boot/zImage %{buildroot}/boot/
install -m 644 arch/arm/boot/dts/*.dtb %{buildroot}/boot/

install -m 644 System.map %{buildroot}/boot/System.map-%{fullVersion}
install -m 644 .config %{buildroot}/boot/config-%{fullVersion}

# 3. Install modules
make INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=%{buildroot} modules_install KERNELRELEASE=%{fullVersion}

# 4. Install kernel headers
make INSTALL_PATH=%{buildroot} INSTALL_MOD_PATH=%{buildroot} INSTALL_HDR_PATH=%{buildroot}/usr headers_install KERNELRELEASE=%{fullVersion}

# 4.1 Install perf
install -d %{buildroot}
make -s -C tools/perf DESTDIR=%{buildroot} install
install -d  %{buildroot}/usr/bin
install -d  %{buildroot}/usr/libexec
mv %{buildroot}/bin/* %{buildroot}/usr/bin/
mv %{buildroot}/libexec/* %{buildroot}/usr/libexec/
rm %{buildroot}/etc/bash_completion.d/perf

# 5. Restore source and build irectory
tar -xf linux-kernel-build-%{fullVersion}.tar -C %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}
mv %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/%{buildarch} .
mv %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/Kconfig .
rm -rf %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/*
mv %{buildarch} %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/
mv Kconfig      %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/arch/

# 6. Remove files
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name ".tmp_vmlinux*" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name ".gitignore" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name ".*dtb*tmp" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.*tmp" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "vmlinux" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "uImage" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "zImage" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "test-*" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.cmd" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.ko" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.o" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.S" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.s" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -name "*.c" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion} -size 0c -exec rm -f {} \;
find %{buildroot}/usr/include -name "\.install"  -exec rm -f {} \;
find %{buildroot}/usr -name "..install.cmd" -exec rm -f {} \;

# 6.1 Clean Documentation directory
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/Documentation -type f ! -name "Makefile" ! -name "*.sh" ! -name "*.pl" -exec rm -f {} \;

rm -rf %{buildroot}/boot/vmlinux*
rm -rf %{buildroot}/System.map*
rm -rf %{buildroot}/vmlinux*

# 7. Update file permisions
%define excluded_files ! -name "*.h" ! -name "*.cocci" ! -name "*.tst" ! -name "*.y" ! -name "*.in" ! -name "*.gperf" ! -name "*.PL" ! -name "lex*" ! -name "check-perf-tracei.pl" ! -name "*.*shipped" ! -name "*asm-generic" ! -name "Makefile*" ! -name "*.lds" ! -name "mkversion" ! -name "zconf.l" ! -name "README" ! -name "*.py" ! -name "gconf.glade" ! -name "*.cc" ! -name "dbus_contexts" ! -name "*.pm" ! -name "*.xs" ! -name "*.l" ! -name "EventClass.py" ! -name "typemap" ! -name "net_dropmonitor.py"

find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/tools/perf/scripts/ -type f %{excluded_files} -exec chmod 755 {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}/scripts/            -type f %{excluded_files} -exec chmod 755 {} \;
find %{buildroot}/usr                                                           -type f ! -name "check-perf-tracei.pl" -name "*.sh" -name "*.pl" -exec chmod 755 {} \;
find %{buildroot}/lib/modules/ -name "*.ko"                                     -type f -exec chmod 755 {} \;

# 8. Create symbolic links
rm -f %{buildroot}/lib/modules/%{fullVersion}/build
rm -f %{buildroot}/lib/modules/%{fullVersion}/source
ln -sf /usr/src/linux-kernel-build-%{fullVersion} %{buildroot}/lib/modules/%{fullVersion}/build

# 9. Calculate modules.img size
BIN_SIZE=`du -s %{buildroot}/lib/modules | awk {'printf $1;'}`
let BIN_SIZE=${BIN_SIZE}+1024+512

dd if=/dev/zero of=%{buildroot}/boot/modules.img count=${BIN_SIZE} bs=1024
/usr/sbin/mke2fs -t ext4 -F -d %{buildroot}/lib/modules/ %{buildroot}/boot/modules.img

rm -rf %{buildroot}/lib/modules/%{fullVersion}/kernel

%clean
rm -rf %{buildroot}

%files -n %{variant}-linux-kernel-headers
%defattr (-, root, root)
/usr/include

%files -n %{variant}-linux-kernel-modules
/boot/modules.img

%files -n %{variant}-linux-kernel-devel
%defattr (-, root, root)
/usr/src/linux-kernel-build-%{fullVersion}
/lib/modules/%{fullVersion}/modules.*
/lib/modules/%{fullVersion}/build

%files -n %{variant}-linux-kernel
%license COPYING
/boot/zImage
/boot/*.dtb
/boot/System.map*
/boot/config*

%files -n %{variant}-linux-kernel-perf
%license COPYING
/usr/bin/perf
/usr/libexec/perf-core
