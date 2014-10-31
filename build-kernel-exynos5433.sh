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

rm output/linux-4-0-exynos5433-arm64.tar
if ! [ -d output ] ; then
	mkdir output
fi

if ! [ -e .config ] ; then
	make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- defconfig
fi

make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j8
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- dtbs

tools/dtbtool -o output/dt.img arch/arm64/boot/dts/exynos/
cp arch/arm64/boot/Image output/kernel

tools/mkbootimg --kernel output/kernel --ramdisk usr/ramdisk.img --output output/boot.img --dt output/dt.img
cd output
tar cf linux-4-0-exynos5433-arm64.tar boot.img
