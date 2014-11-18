%define config_name tizen_rinato_defconfig
%define buildarch arm
%define target_board rinato
%define variant %{buildarch}-%{target_board}

Name: rinato-linux-kernel
Summary: The Linux Kernel for Samsung Gear2
Version: 3.10.60
Release: 0
License: GPL-2.0
ExclusiveArch: %{arm}
Group: System/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0:   linux-kernel-%{version}.tar.xz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

%define fullVersion %{version}-%{variant}

BuildRequires: bc

%description
The Linux Kernel, the operating system core itself

%package -n %{variant}-linux-kernel
Summary: Tizen kernel for %{target_board}
Group: System/Kernel
Provides: %{variant}-odroid-kernel-profile-%{profile} = %{version}-%{release}
Provides: %{variant}-kernel-uname-r = %{fullVersion}

%description -n %{variant}-linux-kernel
This package contains the Linux kernel for Tizen (%{profile} profile, arch %{buildarch}, target board %{target_board})

%prep
%setup -q -n linux-kernel-%{version}

%build
# Make sure EXTRAVERSION says what we want it to say
sed -i "s/^EXTRAVERSION.*/EXTRAVERSION = -%{release}-%{variant}/" Makefile

# 1. Compile sources
make %{config_name}
make %{?_smp_mflags}

# 2. Build zImage
make zImage %{?_smp_mflags}
make dtbs %{?_smp_mflags}

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 1. Destynation directories
mkdir -p %{buildroot}/var/tmp/boot/

# 2. Install zImage
cat arch/arm/boot/zImage arch/arm/boot/dts/exynos3250-rinato.dtb > %{buildroot}/var/tmp/boot/zImage

rm -rf %{buildroot}/System.map*
rm -rf %{buildroot}/vmlinux*

%clean
rm -rf %{buildroot}

%files -n %{variant}-linux-kernel
%license COPYING
/var/tmp/boot/zImage
