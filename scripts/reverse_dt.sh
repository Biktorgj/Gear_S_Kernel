#!/bin/sh
CHECK_DIR=./arch/arm/boot/dts

for i in ${CHECK_DIR}/*.dtb; do
	echo "${i}"
	./scripts/dtc/dtc -I dtb -O dts ${i} > ${i}_reverse
	echo
	mv ${CHECK_DIR}/*_reverse .
done

ls -al *_reverse

