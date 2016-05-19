#!/bin/bash

# Check this system has ccache
check_ccache()
{
	type ccache
	if [ "$?" -eq "0" ]; then
		CCACHE=ccache
	fi
}

check_ccache

rm -f output/linux-*-exynos5433-arm64.tar
if ! [ -d output ] ; then
	mkdir output
fi

if ! [ -e .config ] ; then
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- tizen_tm2_defconfig
fi

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs

tools/dtbtool -o output/dt.img arch/arm64/boot/dts/exynos/
cp arch/arm64/boot/Image output/kernel

tools/mkbootimg --kernel output/kernel --ramdisk usr/ramdisk.img --output output/boot.img --dt output/dt.img

# Check kernel version from Makefile
_major_version=`cat Makefile | grep "^VERSION = " | awk '{print $3}'`
_minor_version=`cat Makefile | grep "^PATCHLEVEL = " | awk '{print $3}'`
_extra_version=`cat Makefile | grep "^EXTRAVERSION = " | awk '{print $3}'`
_version=${_major_version}.${_minor_version}${_extra_version}

cd output
tar cf linux-${_version}-exynos5433-arm64.tar boot.img
