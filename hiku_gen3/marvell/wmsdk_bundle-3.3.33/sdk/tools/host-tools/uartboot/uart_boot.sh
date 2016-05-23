#!/bin/sh
clear

port_name=$2
input_file=$1
flag=1
#check if File exists
if [ -f $1 ]
then
echo "Input file: $1"
else
echo "Error: File $1 does not exist"
exit 0
fi

#check File format
if [[ ${input_file###*.} =~ axf ]]
then
echo "Input file format:axf"
#convert axf to ihex format
ihex_file="data.hex"
flag=0
arm-none-eabi-objcopy -O ihex $input_file $ihex_file
result=$?

	#ihex conversion status
	if [[ $result -eq 0 ]]
	then
	echo "Ihex conversion successful"
	else
	echo "Ihex conversion for file $input_file failed"
	exit 0
	fi

#Input Format is assumed to be ihex
else
ihex_file=$input_file
fi


# direct input to uartboot utility
./uartboot $ihex_file $port_name
result=$?
if [ $result -eq 0 ]
then
echo " "
else
echo "Error while loading code over UART0"
fi

#delete ihex_file(if created for UARTboot Utility)
if [[ ($flag -eq 0) && ( -f $ihex_file) ]]
then
rm $ihex_file
fi

exit 0
