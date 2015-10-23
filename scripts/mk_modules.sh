#!/bin/bash

let NCPUS=(`grep -c ^processor /proc/cpuinfo` + 1)

if [ "$CROSS_COMPILE" == "" ]; then
	export CROSS_COMPILE=arm-linux-gnueabihf-
fi

MOD_DIR="usr/tmp_mod"
MOD_IMG="usr/modules.img"
MOD_SIZE=32

sudo ls > /dev/null

make ARCH=arm modules_prepare
make ARCH=arm modules -j ${NCPUS}

if [ "$?" != "0" ]; then
	echo "Failed to make modules"
	exit 1
fi

[ -d ${MOD_DIR} ] || mkdir ${MOD_DIR}
make ARCH=arm modules_install INSTALL_MOD_PATH=${MOD_DIR} INSTALL_MOD_STRIP=1

dd if=/dev/zero of=${MOD_IMG} bs=1M count=${MOD_SIZE}

mkfs.ext4 -F -b 4096 -L modules ${MOD_IMG}

[ -d ${MOD_DIR}/mnt ] || mkdir ${MOD_DIR}/mnt
sudo mount -o loop ${MOD_IMG} ${MOD_DIR}/mnt
sudo cp -rf ${MOD_DIR}/lib/modules/* ${MOD_DIR}/mnt
sync
sudo umount ${MOD_DIR}/mnt

rm -rf ${MOD_DIR}

ls -al ${MOD_IMG}
