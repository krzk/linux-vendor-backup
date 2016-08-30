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

rm -f output/linux-*-exynos5433-arm64*.tar
rm -f arch/arm64/boot/Image
rm -f arch/arm64/boot/dts/exynos/*.dtb
if ! [ -d output ] ; then
	mkdir output
fi

if ! [ -e .config ] ; then
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- tizen_tm2_defconfig
fi

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs
if [ ! -f "./arch/arm64/boot/Image" ]; then
	echo "Build fail"
	exit 1
fi

HOST_ARCH=`uname -m`
if [ "$HOST_ARCH" == "x86_64" ]; then
	cp tools/mkimage.x86_64 tools/mkimage
elif [ "$HOST_ARCH" == "i586" ] || [ "$HOST_ARCH" == "i686" ]; then
	cp tools/mkimage.i686 tools/mkimage
else
	echo "Unknow HOST architecture, u-boot-tools, mkimage is required!"
fi

# create fit style image from its
PATH=scripts/dtc:$PATH tools/mkimage -f arch/arm64/boot/tizen-tm2.its output/kernel.img

# Check kernel version from Makefile
_major_version=`cat Makefile | grep "^VERSION = " | awk '{print $3}'`
_minor_version=`cat Makefile | grep "^PATCHLEVEL = " | awk '{print $3}'`
_extra_version=`cat Makefile | grep "^EXTRAVERSION = " | awk '{print $3}'`
_version=${_major_version}.${_minor_version}${_extra_version}

cd output
tar cf linux-${_version}-exynos5433-arm64-fit.tar kernel.img
