#!/bin/bash
#
JOBS="-j2"
RELEASE_DATE=`date +%Y%m%d`
DTS_PREFIX="msm8226-tizen_b3"
BOOT_PATH="arch/arm/boot"
DZIMAGE="dzImage"

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

/usr/bin/qemu-arm-static mkdzimage -o $BOOT_PATH/$DZIMAGE -k $BOOT_PATH/zImage -d $BOOT_PATH/merged-dtb
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
