#!/bin/bash
# mkmod.sh
# Making an image file which contains driver modules(.ko files)
# The image file will be in $TMP_DIR/$BIN_NAME (usr/tmp-mod/modules.img)
# CAUTION: This script MUST be run in the root directory of linux kernel source.
# Author: Wonil Choi (wonil22.choi@samsung.com)

TMP_DIR="usr/tmp-mod"
BIN_NAME="modules.img"

make modules_prepare ARCH=arm
make modules ARCH=arm
if [ "$?" != "0" ]; then
	echo "Failed to make modules"
	exit 1
fi
make modules_install ARCH=arm INSTALL_MOD_PATH=${TMP_DIR}
if [ "$?" != "0" ]; then
	echo "Failed to make modules_install"
	exit 1
fi

# modules image size is dynamically determined
BIN_SIZE=`du -s ${TMP_DIR}/lib | awk {'printf $1;'}`
let BIN_SIZE=${BIN_SIZE}+2048 # Additional 2 MB for journal + buffer

if [ -f ./scripts/make_ext4fs ]; then
	echo "Make modules.img using by make_ext4fs tool"
	./scripts/make_ext4fs -l ${BIN_SIZE}K -b 1024 ${TMP_DIR}/${BIN_NAME} ${TMP_DIR}/lib/modules/
	if [ "$?" != "0" ]; then
		echo "Failed to make modules.img(using by make_ext4fs)"
		exit 1
	fi
	rm ${TMP_DIR}/lib -rf
else
	cd ${TMP_DIR}/lib
	mkdir -p tmp
	dd if=/dev/zero of=${BIN_NAME} count=${BIN_SIZE} bs=1024
	mkfs.ext4 -q -F -t ext4 -b 1024 ${BIN_NAME}
	sudo -n mount -t ext4 ${BIN_NAME} ./tmp -o loop
	if [ "$?" != "0" ]; then
		echo "Failed to mount (or sudo)"
		exit 1
	fi
	cp modules/* ./tmp -rf
	sudo -n chown root:root ./tmp -R
	sync
	sudo -n umount ./tmp
	if [ "$?" != "0" ]; then
		echo "Failed to umount (or sudo)"
		exit 1
	fi
	mv ${BIN_NAME} ../
	cd ../
	rm lib -rf
fi
