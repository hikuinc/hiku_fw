#!/bin/bash

# Copyright (C) 2015 Marvell International Ltd.
# All Rights Reserved.

# Flashblob wrapper/helper script

SCRIPT_DIR=`dirname $0`
FLASH_ERASED=0
LAYOUT_SIZE_IN_HEX=0x1000
layout_size=`printf "%d" $LAYOUT_SIZE_IN_HEX`

print_usage() {
	echo ""
	echo "Usage: $0 -l <layout.txt> -o <flash.blob>"
	echo "<-l | --new-layout> Flash partition layout file <layout.txt>"
	echo "<-o | --output> Flash blob output file <outflash.blob>"
	echo ""
	echo "Optional Usage:"
	echo "<-i | --input> Flash blob input file <inflash.blob>"
	echo "[-lb | --new-layout-bin] Flash partition layout binary file <layout.bin>"
	echo "[--<component_name> <file_path>] As an alternate to batch mode"
}

while test -n "$1"; do
	case "$1" in
	--input|-i)
		INPUT_FILE=$2
		shift 2
	;;
	--new-layout|-l)
		LAYOUT_FILE=$2
		shift 2
	;;
	--new-layout-bin|-lb)
		LAYOUT_BIN=$2
		shift 2
	;;
	--boot2)
		BOOT2_FILE=$2
		shift 2
	;;
	--mcufw)
		MCUFW_FILE=$2
		shift 2
	;;
	--ftfs)
		FTFS_FILE=$2
		shift 2
	;;
	--wififw)
		WIFIFW_FILE=$2
		shift 2
	;;
	--output|-o)
		OUTPUT_FILE=$2
		shift 2
	;;
	--help|-h)
		print_usage
		exit 1
	;;
	esac
done

# Arguments : $1 = file name, Return Value = file size
file_size() {
	file_size=`wc -c < "$1" | tr -d [:space:]`
	echo $file_size
}

# Arguments : None, Return Value = total flash size
flash_size () {
	last_comp_addr=`sort $LAYOUT_FILE -k 2 -n -r | awk 'FNR == 1 {print $2}'`
	last_comp_size=`sort $LAYOUT_FILE -k 2 -n -r | awk 'FNR == 1 {print $3}'`
	flash_size=`printf "%d" $(($last_comp_addr + $last_comp_size))`
	echo $flash_size
}

# Arguments : $1 = component name, Return Value = component address
component_addr () {
	addr=`awk '$5 ~/'"$1"'/' $LAYOUT_FILE | awk 'FNR == 1 {print $2}'`
	if [ -z $addr ]; then
		echo "Error: "$1" partition is not specified in layout file $LAYOUT_FILE" 1>&2
		exit 1
	fi
	addr=`printf "%d" $addr`
	echo $addr
}

# Arguments : $1 = component name, Return Value = component size
component_size () {
	addr=`awk '$5 ~/'"$1"'/' $LAYOUT_FILE | awk 'FNR == 1 {print $3}'`
	if [ -z $addr ]; then
		echo "Error: "$1" partition is not specified in layout file $LAYOUT_FILE" 1>&2
		exit 1
	fi
	addr=`printf "%d" $addr`
	echo $addr
}

# Arguments : $1 = component address, $2 = component size
flash_erase () {
	if [ $FLASH_ERASED -eq 1 ]; then
		return
	fi
	echo "Erasing flash at addr "$1" for "$2" bytes..."
	dd if=/dev/zero ibs="$2" count=1 | tr "\000" "\377" | dd obs=1 seek=$addr of=$OUTPUT_FILE conv=notrunc
}

# Arguments : $1 = component name, $2 = component file
flash_component() {
	if [ ! -e "$2" ]; then
		echo "Error: "$1" file "$2" does not exist"
		exit 1
	fi
	addr=$(component_addr "$1")
	if [ -z $addr ]; then
		exit 1
	fi
	size=$(component_size "$1")
	if [ -z $size ]; then
		exit 1
	fi
	if [ "$1" == "boot2" ]; then
		size=`printf "%d" $(($size - $layout_size - $layout_size))`
	fi
	if [ $size -lt $(file_size "$2") ]; then
		echo "Error: "$2" size is larger than partition "$1" size"
		exit 1
	fi

	flash_erase $addr $size
	echo "Flashing "$1" file "$2" at $addr..."
	dd if="$2" obs=1 seek=$addr of=$OUTPUT_FILE conv=notrunc
}

if [ -z $LAYOUT_FILE ]; then
	echo "Error: Layout file is not specified"
	exit 1
fi
if [ ! -e $LAYOUT_FILE ]; then
	echo "Error: Layout file $LAYOUT_FILE does not exist"
	exit 1
fi

if [ -z $OUTPUT_FILE ]; then
	echo "Error: Output file is not specified"
	exit 1
fi

if [ -z $INPUT_FILE ] ; then
	# create 0xFF padded output file
	size=$(flash_size)
	echo "Erasing flash for $size bytes..."
	dd if=/dev/zero ibs=$size count=1 | tr "\000" "\377" >$OUTPUT_FILE
	FLASH_ERASED=1
else
	# copy input file
	if [ ! -e $INPUT_FILE ]; then
		echo "Error: Input file $INPUT_FILE does not exist"
		exit 1
	fi
	if [ $(file_size $INPUT_FILE) != $(flash_size) ]; then
		echo "Error: Input file $INPUT_FILE has incorrect layout"
		exit 1
	fi
	cp $INPUT_FILE $OUTPUT_FILE
	FLASH_ERASED=0
fi

# boot2 flashing
if [ ! -z $BOOT2_FILE ]; then
	flash_component boot2 $BOOT2_FILE
fi

# layout flashing
if [ ! -z $LAYOUT_BIN ]; then
	if [ ! -e $LAYOUT_BIN ]; then
		echo "Error: Layout bin file $LAYOUT_BIN does not exist"
		exit 1
	fi
	if [ $layout_size -lt $(file_size $LAYOUT_BIN) ]; then
		echo "Error: $LAYOUT_BIN size is larger than partition layout size"
		exit 1
	fi
	boot2_addr=$(component_addr boot2)
	boot2_size=$(component_size boot2)

	addr=`printf "%d" $(($boot2_add + $boot2_size - $layout_size - $layout_size))`
	flash_erase $addr $layout_size
	echo "Flashing layout bin file $LAYOUT_BIN at $addr..."
	dd if=$LAYOUT_BIN obs=1 seek=$addr of=$OUTPUT_FILE conv=notrunc

	addr=`printf "%d" $(($addr + $layout_size))`
	flash_erase $addr $layout_size
	echo "Flashing layout bin file $LAYOUT_BIN at $addr..."
	dd if=$LAYOUT_BIN obs=1 seek=$addr of=$OUTPUT_FILE conv=notrunc
fi

# mcufw flashing
if [ ! -z $MCUFW_FILE ]; then
	flash_component mcufw $MCUFW_FILE
fi

# ftfs flashing
if [ ! -z $FTFS_FILE ]; then
	flash_component ftfs $FTFS_FILE
fi

# wififw flashing
if [ ! -z $WIFIFW_FILE ]; then
	flash_component wififw $WIFIFW_FILE
fi
