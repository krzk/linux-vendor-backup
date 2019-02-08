#!/bin/bash
#
JOBS=`grep -c processor /proc/cpuinfo`
let JOBS=${JOBS}*2
JOBS="-j${JOBS}"
RELEASE_DATE=`date +%Y%m%d`
COMMIT_ID=`git log --pretty=format:'%h' -n 1`
ARM=arm64
BOOT_PATH="arch/${ARM}/boot"
IMAGE="Image"
DZIMAGE="dzImage"

HOST_OS=`uname -m`
export CROSS_COMPILE="/usr/bin/aarch64-linux-gnu-"

if ! [ -e .config ] ; then
	make ARCH=${ARM} tizen_tw3_defconfig
fi

if [ "$?" != "0" ]; then
	echo "Failed to make defconfig :"$ARCH
	exit 1
fi

rm ${BOOT_PATH}/dts/exynos/*.dtb -f
rm ${BOOT_PATH}/merged-dtb -f
rm ${BOOT_PATH}/${DZIMAGE}.u -f

make ${JOBS} ARCH=${ARM} ${IMAGE}
if [ "$?" != "0" ]; then
	echo "Failed to make "${IMAGE}
	exit 1
fi

DTC_PATH="scripts/dtc/"

./scripts/exynos_dtbtool.sh -o ${BOOT_PATH}/merged-dtb -p ${DTC_PATH} -v ${BOOT_PATH}/dts/exynos/
if [ "$?" != "0" ]; then
	echo "Failed to make merged-dtb"
	exit 1
fi

./scripts/exynos_mkdzimage.sh -o ${BOOT_PATH}/${DZIMAGE} -k ${BOOT_PATH}/${IMAGE} -d ${BOOT_PATH}/merged-dtb
if [ "$?" != "0" ]; then
	echo "Failed to make mkdzImage"
	exit 1
fi

RELEASE_IMAGE=System_tizen_tw3_${RELEASE_DATE}-${COMMIT_ID}.tar

tar cf ${RELEASE_IMAGE} -C ${BOOT_PATH} ${DZIMAGE}
if [ "$?" != "0" ]; then
	echo "Failed to tar ${DZIMAGE}"
	exit 1
fi

echo ${RELEASE_IMAGE}
