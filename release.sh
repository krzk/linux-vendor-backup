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
MODEL=${1}
SIZE=${2}
CARRIER=${3}
REGION=${4}
OPERATOR=${5}
TIZEN_MODEL=tizen_${MODEL}

HOST_OS=`uname -m`
if [ $HOST_OS = "x86_64" ]; then
	#toolchain for 64bit HOST OS
	export CROSS_COMPILE="/usr/bin/aarch64-linux-gnu-"
else
	echo "Tizen4.0 only support 64bit environment. Please try to build with 64bit environment"
	exit 0
fi

if [ "${MODEL}" = "" ]; then
	echo "Warnning: failed to get machine id."
	echo "ex)./release.sh model_name region_name"
	echo "ex)--------------------------------------------------"
	echo "ex)./release.sh galileo large"
	echo "ex)./release.sh galileo large lte na"
	echo "ex)./release.sh galileo large lte kor"
	echo "ex)./release.sh galileo large lte eur"
	echo "ex)./release.sh galileo large lte chn"
	echo "ex)./release.sh galileo small"
	echo "ex)./release.sh galileo small lte na"
	echo "ex)./release.sh galileo small lte kor"
	echo "ex)./release.sh galileo small lte eur"
	exit
fi

if [ "${SIZE}" != "" ]; then
	VARIANT="${SIZE}"
	if [ "${CARRIER}" != "" ]; then
		VARIANT="${VARIANT}_${CARRIER}"
	fi
else
	if [ "${MODEL}" = "galileo" ]; then
		VARIANT="large_lte"
	fi
fi

if ! [ -e .config ] ; then
	if [ "${VARIANT}" = "" ]; then
		make ARCH=${ARM} ${TIZEN_MODEL}_defconfig
	else
		make ARCH=${ARM} ${TIZEN_MODEL}_defconfig VARIANT_DEFCONFIG=${TIZEN_MODEL}_${VARIANT}_defconfig
	fi
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

if [ "${REGION}" != "" ]; then
    VARIANT="${VARIANT}_${REGION}"
fi

if [ "${VARIANT}" != "" ]; then
	RELEASE_IMAGE=System_${TIZEN_MODEL}_${VARIANT}_${RELEASE_DATE}-${COMMIT_ID}.tar
else
	RELEASE_IMAGE=System_${TIZEN_MODEL}_${RELEASE_DATE}-${COMMIT_ID}.tar
fi

tar cf ${RELEASE_IMAGE} -C ${BOOT_PATH} ${DZIMAGE}
if [ "$?" != "0" ]; then
	echo "Failed to tar ${DZIMAGE}"
	exit 1
fi

echo ${RELEASE_IMAGE}
