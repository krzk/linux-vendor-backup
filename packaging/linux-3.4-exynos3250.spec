%define CHIPSET exynos3250
%define KERNEL_VERSION 3.4

Name: linux-%{KERNEL_VERSION}-%{CHIPSET}
Summary: The Linux Kernel
Version: Tizen_exynos3250_20150821_4_468f84a5
Release: 4
License: GPL
Group: System Environment/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0: %{name}-%{version}.tar.gz

BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root
Provides: linux-%{KERNEL_VERSION}
%define __spec_install_post /usr/lib/rpm/brp-compress || :
%define debug_package %{nil}

BuildRequires:  lzop
BuildRequires:  binutils-devel
BuildRequires:  module-init-tools elfutils-devel
BuildRequires:	python
BuildRequires:	gcc
BuildRequires:	bash
BuildRequires:	system-tools
BuildRequires:	sec-product-features
BuildRequires:	bc
ExclusiveArch:  %arm

%description
The Linux Kernel, the operating system core itself

%if "%{?sec_product_feature_kernel_defconfig}" == "undefined"
%define MODEL tizen_wc1
%else
%define MODEL tizen_%{?sec_product_feature_kernel_defconfig}
%endif

%package -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}
License: GPL
Summary: Linux support headers for userspace development
Group: System Environment/Kernel
Requires(post): coreutils

%files -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}
/var/tmp/kernel/mod_%{MODEL}
/var/tmp/kernel/kernel-%{MODEL}/zImage
/var/tmp/kernel/kernel-%{MODEL}/zImage-recovery

%post -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}
mv /var/tmp/kernel/mod_%{MODEL}/lib/modules/* /lib/modules/.
mv /var/tmp/kernel/kernel-%{MODEL}/zImage /var/tmp/kernel/.
mv /var/tmp/kernel/kernel-%{MODEL}/zImage-recovery /var/tmp/kernel/.

%description -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}
This package provides the %{CHIPSET}_eur linux kernel image & module.img.

%package -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}-debuginfo
License: GPL
Summary: Linux support debug symbol
Group: System Environment/Kernel

%files -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}-debuginfo
/var/tmp/kernel/mod_%{MODEL}
/var/tmp/kernel/kernel-%{MODEL}

%description -n linux-%{KERNEL_VERSION}-%{CHIPSET}_%{MODEL}-debuginfo
This package provides the %{CHIPSET}_eur linux kernel's debugging files.

%package -n kernel-headers-%{KERNEL_VERSION}-%{CHIPSET}
License: GPL
Summary: Linux support headers for userspace development
Group: System Environment/Kernel
Provides: kernel-headers, kernel-headers-tizen-dev
Obsoletes: kernel-headers

%description -n kernel-headers-%{KERNEL_VERSION}-%{CHIPSET}
This package provides userspaces headers from the Linux kernel. These
headers are used by the installed headers for GNU glibc and other system
 libraries.

%package -n kernel-devel-%{KERNEL_VERSION}-%{CHIPSET}
License: GPL
Summary: Linux support kernel map and etc for other package
Group: System/Kernel
Provides: kernel-devel-tizen-dev

%description -n kernel-devel-%{KERNEL_VERSION}-%{CHIPSET}
This package provides kernel map and etc information.

%package -n linux-kernel-license
License: GPL
Summary: Linux support kernel license file
Group: System/Kernel

%description -n linux-kernel-license
This package provides kernel license file.

%prep
%setup -q

%build
%if 0%{?tizen_build_binary_release_type_eng}
%define RELEASE eng
%else
%define RELEASE usr
%endif

mkdir -p %{_builddir}/mod_%{MODEL}
make distclean

./release_obs.sh %{RELEASE} %{MODEL}

cp -f arch/arm/boot/zImage %{_builddir}/zImage.%{MODEL}
cp -f arch/arm/boot/zImage %{_builddir}/zImage-recovery.%{MODEL}
cp -f System.map %{_builddir}/System.map.%{MODEL}
cp -f .config %{_builddir}/config.%{MODEL}
cp -f vmlinux %{_builddir}/vmlinux.%{MODEL}

make modules
make modules_install INSTALL_MOD_PATH=%{_builddir}/mod_%{MODEL}

#remove all changed source codes for next build
cd %_builddir
rm -rf %{name}-%{version}
/bin/tar -xf %{SOURCE0}
cd %{name}-%{version}

%install
mkdir -p %{buildroot}/usr
make mrproper
make headers_check
make headers_install INSTALL_HDR_PATH=%{buildroot}/usr

find  %{buildroot}/usr/include -name ".install" | xargs rm -f
find  %{buildroot}/usr/include -name "..install.cmd" | xargs rm -f
rm -rf %{buildroot}/usr/include/scsi
rm -f %{buildroot}/usr/include/asm*/atomic.h
rm -f %{buildroot}/usr/include/asm*/io.h

mkdir -p %{buildroot}/usr/share/license
cp -vf COPYING %{buildroot}/usr/share/license/linux-kernel

mkdir -p %{buildroot}/var/tmp/kernel/kernel-%{MODEL}
mkdir -p %{buildroot}/var/tmp/kernel/license-%{MODEL}

mv %_builddir/mod_%{MODEL} %{buildroot}/var/tmp/kernel/mod_%{MODEL}

mv %_builddir/zImage.%{MODEL} %{buildroot}/var/tmp/kernel/kernel-%{MODEL}/zImage
mv %_builddir/zImage-recovery.%{MODEL} %{buildroot}/var/tmp/kernel/kernel-%{MODEL}/zImage-recovery

mv %_builddir/System.map.%{MODEL} %{buildroot}/var/tmp/kernel/kernel-%{MODEL}/System.map
mv %_builddir/config.%{MODEL} %{buildroot}/var/tmp/kernel/kernel-%{MODEL}/config
mv %_builddir/vmlinux.%{MODEL} %{buildroot}/var/tmp/kernel/kernel-%{MODEL}/vmlinux

find %{buildroot}/var/tmp/kernel/ -name 'System.map' > develfiles.pre # for secure storage
find %{buildroot}/var/tmp/kernel/ -name 'vmlinux' >> develfiles.pre   # for TIMA
find %{buildroot}/var/tmp/kernel/ -name '*.ko' >> develfiles.pre      # for TIMA
find %{buildroot}/var/tmp/kernel/ -name '*zImage' >> develfiles.pre   # for Trusted Boot
cat develfiles.pre | sed -e "s#%{buildroot}##g" | uniq | sort > develfiles

%clean
rm -rf %_builddir

%files -n kernel-headers-%{KERNEL_VERSION}-%{CHIPSET}
/usr/include/*

%files -n linux-kernel-license
/usr/share/license/*

%files -n kernel-devel-%{KERNEL_VERSION}-%{CHIPSET} -f develfiles
