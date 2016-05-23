#!/bin/sh

PROF_FILE=profile
IMG_FILE=img_file
HEX_FILE=hex_file
SYM_FILE=symbols
TMP_FILE=temp

CURDIR=`pwd`
WMSDK_PATH=$CURDIR/../../

NM=arm-none-eabi-nm
OPENOCD=$WMSDK_PATH/tools/mc200/OpenOCD

if [ -f $WMSDK_PATH/.config ]; then
	source $WMSDK_PATH/.config
else
	echo "Config file not present"
fi

if [ $# -lt 1 ]; then
	echo "Error: Please provide elf file"
	exit 1
fi

if [ $# -eq 2 ]; then
	OPENOCD=$2
fi

# Sort output of nm utility
$NM -a $1| sort -t ' ' -k 1 > $SYM_FILE

# Get the function table address
func_tbl_address=`grep ftable < $SYM_FILE | cut -d" " -f1`
func_tbl_address=`echo "0x$func_tbl_address"`

# Get dump of function address's table
MEM_DUMP_SIZE=$((($CONFIG_PROFILER_FUNCTION_CNT + 1) * 5))
$OPENOCD/bin/linux/openocd -f $OPENOCD/interface/ftdi.cfg -f $OPENOCD/target/mc200.cfg -c init -c "dump_image $IMG_FILE $func_tbl_address $MEM_DUMP_SIZE" -c exit

# Get output of openocd dump in a readable hex format
hexdump -v -e '/1 "%02X "' $IMG_FILE > $HEX_FILE

words=`wc -w < $HEX_FILE`
loop=`expr $words - 4`

if [ -f $PROF_FILE ]; then
	rm $PROF_FILE
fi

for (( i = 1; i < $loop; i=i+5 ))
do
	cnt=`expr $i + 4`
	msb3=`expr $i + 3`
	msb2=`expr $i + 2`
	msb1=`expr $i + 1`
	msb3=`cut --fields="$msb3" -d" " < $HEX_FILE`
	msb2=`cut --fields="$msb2" -d" " < $HEX_FILE`
	msb1=`cut --fields="$msb1" -d" " < $HEX_FILE`
	msb0=`cut --fields="$i" -d" " < $HEX_FILE`
	cnt=`cut --fields="$cnt" -d" " < $HEX_FILE`
	addr=`echo "$msb3$msb2$msb1$msb0"`
	# Stop if 0x00000000 address is found
	if [ $addr == "00000000" ]; then
		break
	fi

	# Mask last 4 bits of address to get the function name of corresponding address.
	search_addr=${addr:0:7}
	search_addr=`echo "$search_addr" | tr '[:upper:]' '[:lower:]'`
	func_name=`grep $search_addr $SYM_FILE`

	# If function name is not found in the earlier step, then mask last 8 bits
	# and find the name from the list of matched addresses.
	if [ $? == 1 ]; then
		search_addr=${addr:0:6}
		search_addr=`echo "$search_addr" | tr '[:upper:]' '[:lower:]'`
		func_list=(`grep $search_addr $SYM_FILE | cut -f1 -d" "`)
		length=${#func_list[@]}
		for (( index = 0; index < $length; index++ ))
		do
			dec_list_addr=`echo "$((0x${func_list[$index]}))"`
			dec_addr=`echo "$((0x$addr))"`
			if [ "$dec_list_addr" -gt "$dec_addr" ];then
				index=`expr $index - 1`
				func_name=`grep ${func_list[$index]} $SYM_FILE`
				break
			fi
			if (( index == length-1 )); then
				func_name=`grep ${func_list[$index]} $SYM_FILE`
			fi
		done
	fi

	func_name=`echo "$func_name" | head -1 | awk -F " " '{print $NF}'`
	echo "$addr $((0x$cnt)) $func_name" >> $PROF_FILE
done

sort -t ' '  -k 2 -n -o $TMP_FILE < $PROF_FILE
mv $TMP_FILE $PROF_FILE

# remove temp files
rm $IMG_FILE $SYM_FILE $HEX_FILE
