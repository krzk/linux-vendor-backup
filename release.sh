#!/bin/bash
#
JOBS=`grep -c processor /proc/cpuinfo`
let JOBS=${JOBS}*2
JOBS="-j${JOBS}"
RELEASE_DATE=`date +%Y%m%d`
COMMIT_ID=`git log --pretty=format:'%h' -n 1`
BOOT_PATH="arch/arm/boot"
ZIMAGE="zImage"
MODEL=${1}
IMG_NAME=${2}
TIZEN_MODEL=tizen_${MODEL}

if [ "${MODEL}" = "" ]; then
	echo "Warnning: failed to get machine id."
	echo "ex)./release.sh model_name"
	echo "ex)--------------------------------------------------"
	echo "ex)./release.sh wc1"
	echo "ex)./release.sh hwp"
	exit
fi

make ARCH=arm ${TIZEN_MODEL}_defconfig
if [ "$?" != "0" ]; then
	echo "Failed to make defconfig :"$ARCH
	exit 1
fi

make ${JOBS} zImage ARCH=arm
if [ "$?" != "0" ]; then
	echo "Failed to make zImage"
	exit 1
fi

RELEASE_IMAGE=System_${MODEL}_${RELEASE_DATE}-${COMMIT_ID}.tar

tar cf ${RELEASE_IMAGE} -C ${BOOT_PATH} ${ZIMAGE}
if [ "$?" != "0" ]; then
	echo "Failed to tar ${ZIMAGE}"
	exit 1
fi

echo ${RELEASE_IMAGE}
