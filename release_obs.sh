#!/bin/bash
#
JOBS=`grep -c processor /proc/cpuinfo`
let JOBS=${JOBS}*2
JOBS="-j${JOBS}"
BOOT_PATH="arch/arm/boot"
DZIMAGE="dzImage"

REL_TYPE_STR=${1}
BASE_DEF_STR=${2}

if [ "${3}" = "" ]; then
	echo "base_def : ${BASE_DEF_STR}_defconfig , Release : ${REL_TYPE_STR}"
else
	VARI_DEF_STR=${2}_${3}
	echo "base_def : ${BASE_DEF_STR}_defconfig , variant_def : ${VARI_DEF_STR}_defconfig, Release : ${REL_TYPE_STR}"
fi

DTS_PREFIX="msm8226-${BASE_DEF_STR}"

if [ "${REL_TYPE_STR}" = "USR" ]; then
	echo "Now disable CONFIG_SLP_KERNEL_ENG for ${BASE_DEF_STR}_defconfig"

	if [ "${2}" = "tizen_b3_cdma" ]; then
		sed -i 's/CONFIG_SLP_KERNEL_ENG=y/\# CONFIG_SLP_KERNEL_ENG is not set/g' arch/arm/configs/tizen_b3_defconfig
	else
		sed -i 's/CONFIG_SLP_KERNEL_ENG=y/\# CONFIG_SLP_KERNEL_ENG is not set/g' arch/arm/configs/${BASE_DEF_STR}_defconfig
	fi
	if [ "$?" != "0" ]; then
		echo "Failed to disable CONFIG_SLP_KERNEL_ENG feature"
		exit 1
	fi
fi

if [ "${3}" = "" ]; then
	make ARCH=arm ${BASE_DEF_STR}_defconfig
elif [ "${2}" = "tizen_b3_cdma" ]; then
	make ARCH=arm tizen_b3_defconfig VARIANT_DEFCONFIG=${VARI_DEF_STR}_defconfig
else
	make ARCH=arm ${BASE_DEF_STR}_defconfig VARIANT_DEFCONFIG=${VARI_DEF_STR}_defconfig
fi

if [ "$?" != "0" ]; then
	echo "Failed to make defconfig"
	exit 1
fi

make $JOBS zImage ARCH=arm
if [ "$?" != "0" ]; then
	echo "Failed to make zImage"
	exit 1
fi

DTC_PATH="scripts/dtc/"
DTC_EXEC=$DTC_PATH"dtc"
DTS_FILES="$BOOT_PATH/dts/$DTS_PREFIX"
DTS_LIST=dts_list

rm $BOOT_PATH/dts/*.dtb -f
ls $DTS_FILES-*.dts >> $DTS_LIST

for DTS_FILE in $(cat $DTS_LIST)
do
	DTB_FILE=$(echo $DTS_FILE | sed 's/.dts/.dtb/2')
	$DTC_EXEC -p 1024 -O dtb -o ${DTB_FILE} ${DTS_FILE}
	if [ "$?" != "0" ]; then
		echo "Failed to make each dtb"
		exit 1
	fi
done

rm $DTS_LIST -f

dtbtool -o $BOOT_PATH/merged-dtb -p $DTC_PATH -v $BOOT_PATH/dts/
if [ "$?" != "0" ]; then
	echo "Failed to make merged-dtb"
	exit 1
fi

mkdzimage -o $BOOT_PATH/$DZIMAGE -k $BOOT_PATH/zImage -d $BOOT_PATH/merged-dtb
if [ "$?" != "0" ]; then
	echo "Failed to make mkdzImage"
	exit 1
fi
