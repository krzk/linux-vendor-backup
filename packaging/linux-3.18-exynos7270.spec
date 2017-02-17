%define CHIPSET exynos7270
%define KERNEL_VERSION 3.18
%define MODEL tw2

Name: linux-%{KERNEL_VERSION}-%{CHIPSET}
Summary: The Linux Kernel
Version: 3.18.14
Release: 1
License: GPL-2.0
ExclusiveArch: aarch64
Group: System/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root
Provides: linux-%{KERNEL_VERSION}
%define __spec_install_post /usr/lib/rpm/brp-compress || :
%define debug_package %{nil}

BuildRequires: bc

%description
The Linux Kernel, the operating system core itself

%package -n linux-%{CHIPSET}-%{MODEL}
License: GPL-2.0
Summary: Linux support headers for userspace development
Group: System/Kernel
Requires(post): coreutils

%description -n linux-%{CHIPSET}-%{MODEL}
This package provides the %{CHIPSET}_eur linux kernel image.

%package -n linux-%{CHIPSET}-%{MODEL}-debuginfo
License: GPL-2.0
Summary: Linux support debug symbol
Group: System/Kernel

%description -n linux-%{CHIPSET}-%{MODEL}-debuginfo
This package provides the %{CHIPSET}_eur linux kernel's debugging files.

%package -n kernel-headers-%{CHIPSET}-%{MODEL}
License: GPL-2.0
Summary: Linux support headers for userspace development
Group: System/Kernel
Provides: kernel-headers-tizen-dev

%description -n kernel-headers-%{CHIPSET}-%{MODEL}
This package provides userspaces headers from the Linux kernel. These
headers are used by the installed headers for GNU glibc and other system
 libraries.

%package -n kernel-devel-%{CHIPSET}-%{MODEL}
License: GPL-2.0
Summary: Linux support kernel map and etc for other package
Group: System/Kernel
Provides: kernel-devel-tizen-dev
Provides: kernel-devel-tizen

%description -n kernel-devel-%{CHIPSET}-%{MODEL}
This package provides kernel map and etc information.

%package -n linux-kernel-license-%{CHIPSET}-%{MODEL}
License: GPL-2.0
Summary: Linux support kernel license file
Group: System/Kernel

%description -n linux-kernel-license-%{CHIPSET}-%{MODEL}
This package provides kernel license file.

%prep
%setup -q

%build

make distclean

chmod a+x release_obs.sh
chmod a+x ./scripts/exynos_dtbtool.sh
chmod a+x ./scripts/exynos_mkdzimage.sh

# 1. make kernel image
./release_obs.sh

%install

# 2. copy to buildroot
mkdir -p %{buildroot}/usr
mkdir -p %{buildroot}/usr/share/license
mkdir -p %{buildroot}/boot/kernel/devel

cp -f arch/arm64/boot/dzImage  %{buildroot}/boot/kernel/dzImage
cp -f arch/arm64/boot/merged-dtb  %{buildroot}/boot/kernel/merged-dtb
cp -f arch/arm64/boot/Image  %{buildroot}/boot/kernel/Image
cp -f System.map  %{buildroot}/boot/kernel/System.map
cp -f .config  %{buildroot}/boot/kernel/config
cp -f vmlinux  %{buildroot}/boot/kernel/vmlinux
cp -f COPYING %{buildroot}/usr/share/license/linux-kernel

# 3. make kernel header
make mrproper
make headers_check
make headers_install INSTALL_HDR_PATH=%{buildroot}/usr

# 4. remove unnecessary files.
find  %{buildroot}/usr/include -name ".install" -delete
find  %{buildroot}/usr/include -name "..install.cmd" -delete
rm -rf %{buildroot}/usr/include/scsi
rm -f %{buildroot}/usr/include/asm*/atomic.h
rm -f %{buildroot}/usr/include/asm*/io.h

find %{_builddir}/%{name}-%{version} -name ".tmp_vmlinux*" -delete
find %{_builddir}/%{name}-%{version} -name "\.*dtb*tmp" -delete
find %{_builddir}/%{name}-%{version} -name "*\.*tmp" -delete
find %{_builddir}/%{name}-%{version} -name "vmlinux" -delete
find %{_builddir}/%{name}-%{version} -name "bzImage" -delete
find %{_builddir}/%{name}-%{version} -name "zImage" -delete
find %{_builddir}/%{name}-%{version} -name "dzImage" -delete
find %{_builddir}/%{name}-%{version} -name "*.cmd" -delete
find %{_builddir}/%{name}-%{version} -name "*\.ko" -delete
find %{_builddir}/%{name}-%{version} -name "*\.o" -delete
find %{_builddir}/%{name}-%{version} -name "*\.S" -delete
find %{_builddir}/%{name}-%{version} -name "*\.c" -not -path "%{_builddir}/%{name}-%{version}/scripts/*" -delete

# 5. make kernel-devel
mv %{_builddir}/%{name}-%{version} %{buildroot}/boot/kernel/devel/kernel-devel-%{MODEL}
mkdir -p %{_builddir}/%{name}-%{version}
ln -s kernel-devel-%{MODEL} %{buildroot}/boot/kernel/devel/tizen-devel

%files -n linux-%{CHIPSET}-%{MODEL}
/boot/kernel/dzImage

%files -n linux-%{CHIPSET}-%{MODEL}-debuginfo
/boot/kernel/config
/boot/kernel/Image
/boot/kernel/merged-dtb
/boot/kernel/System.map
/boot/kernel/vmlinux

%files -n kernel-headers-%{CHIPSET}-%{MODEL}
%defattr(644,root,root,-)
/usr/include/*

%files -n linux-kernel-license-%{CHIPSET}-%{MODEL}
/usr/share/license/*

%files -n kernel-devel-%{CHIPSET}-%{MODEL}
%defattr(644,root,root,-)
/boot/kernel/devel/*
