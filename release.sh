#!/bin/bash
#
JOBS=`grep -c processor /proc/cpuinfo`
let JOBS=${JOBS}*2
JOBS="-j${JOBS}"
RELEASE_DATE=`date +%Y%m%d`
COMMIT_ID=`git log --pretty=format:'%h' -n 1`
DTS_PREFIX="msm8226-${1}"
BOOT_PATH="arch/arm/boot"
DZIMAGE="dzImage"

if [ "${1}" = "" ]; then
	echo "Warnning: failed to get machine id."
	echo "ex)./release.sh def_name_prefix locale"
	echo "ex)./release.sh tizen_w3g"
	echo "ex)./release.sh tizen_b3"
	echo "ex)./release.sh tizen_b3 att"
	echo "ex)./release.sh tizen_b3 tmo"
	echo "ex)./release.sh tizen_b3 vmc"
	echo "ex)./release.sh tizen_b3_cdma"
	echo "ex)./release.sh tizen_b3_cdma spr"
	echo "ex)./release.sh tizen_b3_cdma vzw"
	echo "ex)./release.sh tizen_b3_cdma usc"
	exit
fi

if [ "${2}" = "" ]; then
	make ARCH=arm ${1}_defconfig
elif [ "${1}" = "tizen_b3_cdma" ]; then
	make ARCH=arm tizen_b3_defconfig VARIANT_DEFCONFIG=${1}_${2}_defconfig
else
	make ARCH=arm ${1}_defconfig VARIANT_DEFCONFIG=${1}_${2}_defconfig
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

sudo ls > /dev/null
./scripts/mkmodimg.sh

if [ "${2}" != "" ]; then
	RELEASE_IMAGE=System_msm8x26_${1}_${2}_${RELEASE_DATE}-${COMMIT_ID}.tar
else
	RELEASE_IMAGE=System_msm8x26_${1}_${RELEASE_DATE}-${COMMIT_ID}.tar
fi

tar cf $RELEASE_IMAGE -C $BOOT_PATH $DZIMAGE
if [ "$?" != "0" ]; then
	echo "Failed to tar $DZIMAGE"
	exit 1
fi

tar rf $RELEASE_IMAGE -C usr/tmp-mod modules.img
if [ "$?" != "0" ]; then
	echo "Failed to tar modules.img"
	exit 1
fi
echo $RELEASE_IMAGE
