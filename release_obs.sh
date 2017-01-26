#!/bin/bash
#
JOBS=`grep -c processor /proc/cpuinfo`
let JOBS=${JOBS}*2
JOBS="-j${JOBS}"
ARM=arm64
BOOT_PATH="arch/${ARM}/boot"
IMAGE="Image"
DZIMAGE="dzImage"

CHIPSET=exynos7270
RELEASE=${1}
MODEL=${2}
CARRIER=${3}
REGION=${4}
OPERATOR=${5}
SHOWCASE=${6}
VARIANT="${MODEL}"

if [ "${CARRIER}" != "" ]; then
	VARIANT="${VARIANT}_${CARRIER}"
fi
if [ "${REGION}" != "" ]; then
	VARIANT="${VARIANT}_${REGION}"
fi
if [ "${OPERATOR}" != "" ]; then
	VARIANT="${VARIANT}_${OPERATOR}"
fi

if [ "${VARIANT}" == "${MODEL}" ]; then
	echo "base_def : ${MODEL}_defconfig , Release : ${RELEASE}"
else
	echo "base_def : ${MODEL}_defconfig , variant_def : ${VARIANT}_defconfig, Release : ${RELEASE}"
fi

if [ "${RELEASE}" = "usr" ]; then
	echo "Now disable CONFIG_TIZEN_SEC_KERNEL_ENG for ${MODEL}_defconfig"

	sed -i 's/CONFIG_TIZEN_SEC_KERNEL_ENG=y/\# CONFIG_TIZEN_SEC_KERNEL_ENG is not set/g' arch/${ARM}/configs/${MODEL}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to disable CONFIG_TIZEN_SEC_KERNEL_ENG feature"
		exit 1
	fi

	echo "Now disable CONFIG_DYNAMIC_DEBUG for ${MODEL}_defconfig"

	sed -i 's/CONFIG_DYNAMIC_DEBUG=y/\# CONFIG_DYNAMIC_DEBUG is not set/g' arch/${ARM}/configs/${MODEL}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to disable CONFIG_DYNAMIC_DEBUG feature"
		exit 1
	fi
fi

if [ "${SHOWCASE}" = "ifa" ]; then
	echo "Now enable Showcase config for ${MODEL}_defconfig"

	sed -i 's/\# CONFIG_IFA_DEMO is not set/CONFIG_IFA_DEMO=y/g' arch/${ARM}/configs/${MODEL}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to enable CONFIG_IFA_DEMO feature"
		exit 1
	fi
fi


if [ "${VARIANT}" == "${MODEL}" ]; then
	make ARCH=${ARM} ${MODEL}_defconfig
else
	make ARCH=${ARM} ${MODEL}_defconfig VARIANT_DEFCONFIG=${VARIANT}_defconfig
fi

if [ "$?" != "0" ]; then
	echo "Failed to make defconfig :"${ARCH}
	exit 1
fi

rm ${BOOT_PATH}/dts/*.dtb -f

make ${JOBS} ARCH=${ARM} ${IMAGE}
if [ "$?" != "0" ]; then
	echo "Failed to make "${IMAGE}
	exit 1
fi

DTC_PATH="scripts/dtc/"

dtbtool -o ${BOOT_PATH}/merged-dtb -p ${DTC_PATH} -v ${BOOT_PATH}/dts/
if [ "$?" != "0" ]; then
	echo "Failed to make merged-dtb"
	exit 1
fi

mkdzimage -o ${BOOT_PATH}/${DZIMAGE} -k ${BOOT_PATH}/${IMAGE} -d ${BOOT_PATH}/merged-dtb
if [ "$?" != "0" ]; then
	echo "Failed to make mkdzImage"
	exit 1
fi
