#!/bin/bash
#
JOBS=`grep -c processor /proc/cpuinfo`
let JOBS=${JOBS}*2
JOBS="-j${JOBS}"
BOOT_PATH="arch/arm/boot"
ZIMAGE="zImage"

MODEL=${1}
CONFIG_STR=${MODEL%_smk_dis*}
OPTION_STR=${MODEL#*${CONFIG_STR}}

echo "defconfig : ${CONFIG_STR}_defconfig , option : ${OPTION_STR}"

if [ "${OPTION_STR}" = "_smk_dis" ]; then
	echo "Now change smack-disable for ${CONFIG_STR}_defconfig"

	sed -i 's/CONFIG_SECURITY_SMACK=y/\# CONFIG_SECURITY_SMACK is not set/g' arch/arm/configs/${CONFIG_STR}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to change smack-disable step 1"
		exit 1
	fi

	sed -i 's/\# CONFIG_DEFAULT_SECURITY_DAC is not set/CONFIG_DEFAULT_SECURITY_DAC=y/g' arch/arm/configs/${CONFIG_STR}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to change smack-disable step 2"
		exit 1
	fi

	sed -i 's/CONFIG_DEFAULT_SECURITY_SMACK=y/\# CONFIG_DEFAULT_SECURITY_SMACK is not set/g' arch/arm/configs/${CONFIG_STR}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to change smack-disable step 3"
		exit 1
	fi

	sed -i 's/CONFIG_DEFAULT_SECURITY="smack"/CONFIG_DEFAULT_SECURITY=""/g' arch/arm/configs/${CONFIG_STR}_defconfig
	if [ "$?" != "0" ]; then
		echo "Failed to change smack-disable step 4"
		exit 1
	fi
fi

make ARCH=arm ${CONFIG_STR}_defconfig
if [ "$?" != "0" ]; then
	echo "Failed to make defconfig"
	exit 1
fi

make $JOBS zImage ARCH=arm
if [ "$?" != "0" ]; then
        echo "Failed to make zImage"
        exit 1
fi
