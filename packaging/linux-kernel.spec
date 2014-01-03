%bcond_with abidev
%define config_name tizen_defconfig
%define abiver 1
%define build_id %{config_name}.%{abiver}

Name: linux-kernel
Summary: The Linux Kernel
Version: 3.10.19
Release: 1
License: GPL
Group: System Environment/Kernel
Vendor: The Linux Community
URL: http://www.kernel.org
Source0:   %{name}-%{version}-%{build_id}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{PACKAGE_VERSION}-root

BuildRequires: pkgconfig(glib-2.0)
BuildRequires: pkgconfig(libelf)

BuildRequires: linux-glibc-devel
BuildRequires: u-boot-tools
BuildRequires: bc

%define kernel_build_dir_name .%{name}-%{version}-%{build_id}
%define kernel_build_dir %{_builddir}/%{name}-%{version}/%{kernel_build_dir_name}

%description
The Linux Kernel, the operating system core itself

%if %{with abidev}
%package abi-dev
Summary: Header files for the Linux kernel for use by glibc
Group: Development/System

%description abi-dev
The package provide linux kernel API/ABI development file.
%endif

%package headers
Summary: Header files for the Linux kernel for use by glibc
Group: Development/System
Obsoletes: kernel-headers
Provides: kernel-headers = %{version}

%description headers
Kernel-headers includes the C header files that specify the interface
between the Linux kernel and userspace libraries and programs. The
header files define structures and constants that are needed for
building most standard programs and are also needed for rebuilding the
glibc package.

%package abi-tools
Summary: Kernael ABI tools
Group: Development/System

%description abi-tools
The package provide set of tools to test and create ABI/API dumps.

%package sources
Summary: Full linux kernel sources for out-of-tree modules
Group: Development/System
Provides: kernel-sources = %{version}-%{build_id}

%description sources
Full linux kernel sources for out-of-tree modules.

%package build
Summary: Prebuilt linux kernel for out-of-tree modules
Group: Development/System
Requires: kernel-sources = %{version}-%{build_id}

%description build
Prebuilt linux kernel for out-of-tree modules.

%package uImage
Summary: Linux kernel image
Group: Development/System

%description uImage
Linux kernel uImage

%prep
%setup -q

%build
# 0. Build abi checker
make -C abi-checker/src

# 1. Create main build directory
rm -rf %{kernel_build_dir}
mkdir -p %{kernel_build_dir}

# 2. Create tar archive for sources
tar cpsf %{kernel_build_dir}/linux-kernel-sources-%{version}-%{build_id}.tar . --one-file-system --exclude ".git*"

# 3. Create kernel build directory
mkdir -p %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id}

# 4. Compile sources
make EXTRAVERSION="-%{build_id}" O=%{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id} %{config_name}
make EXTRAVERSION="-%{build_id}" O=%{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id} %{?_smp_mflags}

# 4.1 Test ABI/API kernel change
%if %{with abidev}
echo "No linuks kernel ABI/API checks"
%else
( cd abi-checker/src; chmod 755 build_api_kernel_checker.sh; ./build_api_kernel_checker.sh "%{version}" "%{abiver}" )
%endif

make EXTRAVERSION="-%{build_id}" O=%{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id} uImage %{?jobs:-j%jobs}

# 5. Update Makefile in output build
cat %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id}/Makefile | sed 's/\/home\/abuild\/rpmbuild\/BUILD\/%{name}-%{version}/\/usr\/src\/linux-kernel-sources-%{version}-%{build_id}/' > %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id}/Makefile.new
mv %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id}/Makefile.new %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id}/Makefile
rm -f %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id}/Makefile.new

# 6. Create tar repo for build directory
( cd %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id} ; tar cpsf %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id}.tar . )

%install
QA_SKIP_BUILD_ROOT="DO_NOT_WANT"; export QA_SKIP_BUILD_ROOT

# 1. Destynation directories
mkdir -p %{buildroot}/usr/src/linux-kernel-sources-%{version}-%{build_id}
mkdir -p %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}
mkdir -p %{buildroot}/boot/abi/
mkdir -p %{buildroot}/usr/local/bin

# 2. Install uImage
cp %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id}/arch/arm/boot/uImage %{buildroot}/boot/

# 3. Restore source and build irectory
tar -xf %{kernel_build_dir}/linux-kernel-sources-%{version}-%{build_id}.tar -C %{buildroot}/usr/src/linux-kernel-sources-%{version}-%{build_id}
tar -xf %{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id}.tar   -C %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}

# 4. Install kernel headers
make INSTALL_PATH=%{buildroot}/boot INSTALL_MOD_PATH=%{buildroot} O=%{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id} install
make INSTALL_PATH=%{buildroot} INSTALL_MOD_PATH=%{buildroot} O=%{kernel_build_dir}/linux-kernel-build-%{version}-%{build_id} INSTALL_HDR_PATH=%{buildroot}/usr headers_install

# 4.1 Install ABI/API tools
cp abi-checker/src/abi-checker %{buildroot}/usr/local/bin

# 4.2 Install abi_%{version} file
%if %{with abidev}
find ../.. -name "Module.symvers" -exec cp {} %{buildroot}/boot/abi/abi_devel \;
%else
cp abi-checker/data/abi* %{buildroot}/boot/abi/.
ln -sf /boot/abi/abi_%{version}_%{abiver} %{buildroot}/boot/abi/current
%endif

# 5. Remove files
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name ".tmp_vmlinux1" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name ".tmp_vmlinux2" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "vmlinux" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "uImage" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "zImage" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*.cmd" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.o" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "\.*dtb*tmp" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.*tmp" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.c" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.S" -exec rm -f {} \;
find %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id} -name "*\.ko" -exec rm -f {} \;

find %{buildroot}/usr/src/linux-kernel-sources-%{version}-%{build_id} -name "*.c" -exec rm -f {} \;

find %{buildroot}/usr -name "\.\.install.cmd"  -exec rm -f {} \;
find %{buildroot}/usr -name "\.install"  -exec rm -f {} \;

rm -rf %{buildroot}/usr/src/linux-kernel-sources-%{version}-%{build_id}/%{kernel_build_dir_name}
rm -f  %{buildroot}/usr/src/linux-kernel-sources-%{version}-%{build_id}/source
rm -f  %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}/source

rm -rf %{buildroot}/usr/src/linux-kernel-sources-%{version}-%{build_id}/abi-checker

rm -rf %{buildroot}/System.map*
rm -rf %{buildroot}/vmlinux*

rm -rf %{buildroot}/boot/System.map*
rm -rf %{buildroot}/boot/vmlinux*

# 6. Create symbolic links
ln -sf /usr/src/linux-kernel-sources-%{version}-%{build_id} %{buildroot}/usr/src/linux-kernel-build-%{version}-%{build_id}/source

%clean
rm -rf %{buildroot}

%files headers
%defattr (-, root, root)
/usr/include

%files sources
%defattr (-, root, root)
/usr/src/linux-kernel-sources-%{version}-%{build_id}

%files build
%defattr (-, root, root)
/usr/src/linux-kernel-build-%{version}-%{build_id}

%files uImage
%if %{without abidev}
/boot/
%else
/boot/uImage
%endif

%files abi-tools
/usr/local/bin

%if %{with abidev}
%files abi-dev
/boot/abi
%endif
