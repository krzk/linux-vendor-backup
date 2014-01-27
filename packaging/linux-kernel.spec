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

%define fullVersion %{version}-%{build_id}

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

Provides: kernel = %{version}-%{release}
Provides: kernel-uname-r = %{fullVersion}

%description
The Linux Kernel, the operating system core itself

%package user-headers
Summary: Header files for the Linux kernel for use by glibc
Group: Development/System
Obsoletes: kernel-headers
Provides: kernel-headers = %{version}-%{release}

%description user-headers
Kernel-headers includes the C header files that specify the interface
between the Linux kernel and userspace libraries and programs.  The
header files define structures and constants that are needed for
building most standard programs and are also needed for rebuilding the
glibc package.

%package devel
Summary: Prebuilt linux kernel for out-of-tree modules
Group: Development/System
Provides: kernel-devel = %{fullVersion}
Provides: kernel-devel-uname-r = %{fullVersion}
Requires: %{name} = %{version}-%{release}

%description devel
Prebuilt linux kernel for out-of-tree modules.

%package -n perf
Summary: The 'perf' performance counter tool
Group: Development/System
Provides: perf = %{kernel_full_version}

%description -n perf
This package provides the "perf" tool that can be used to monitor performance
counter events as well as various kernel internal events.

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

# 3.1 Build perf
make -s -C tools/lib/traceevent ARCH=%{buildarch} %{?_smp_mflags}
make -s -C tools/perf WERROR=0 ARCH=%{buildarch}

# 4. Create tar repo for build directory
tar cpsf linux-kernel-build-%{fullVersion}.tar .

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 1. Destynation directories
mkdir -p %{buildroot}/usr/src/linux-kernel-build-%{fullVersion}
mkdir -p %{buildroot}/lib/modules/%{fullVersion}
mkdir -p %{buildroot}/boot/

# 2. Install uImage, System.map, ...
install -m 755 arch/arm/boot/uImage %{buildroot}/boot/
install -m 644 arch/arm/boot/dts/*.dtb %{buildroot}/boot/
mv %{buildroot}/boot/exynos4412-m0.dtb %{buildroot}/boot/exynos4412-trats2.dtb

install -m 644 System.map %{buildroot}/boot/System.map-%{fullVersion}
install -m 644 .config %{buildroot}/boot/config-%{fullVersion}

# 3. Install modules
make INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=%{buildroot} modules_install

# 4. Install kernel headers
make INSTALL_PATH=%{buildroot} INSTALL_MOD_PATH=%{buildroot} INSTALL_HDR_PATH=%{buildroot}/usr headers_install

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

%clean
rm -rf %{buildroot}

%files user-headers
%defattr (-, root, root)
/usr/include

%files devel
%defattr (-, root, root)
/usr/src/linux-kernel-build-%{fullVersion}
/lib/modules/%{fullVersion}/modules.*
/lib/modules/%{fullVersion}/build

%files
%license COPYING
/boot/uImage
/boot/*.dtb
/boot/System.map*
/boot/config*
/lib/modules/%{fullVersion}/kernel
/lib/modules/%{fullVersion}/modules.*

%files -n perf
%license COPYING
/usr/bin/perf
/usr/libexec/perf-core
