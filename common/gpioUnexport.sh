
#!/bin/sh

if [ $# -eq 1 ]
then
        pin=$1
else
        printf "Usage: gpioUnexport gpioNumber\n"
        exit 1
fi

file="/sys/class/gpio/gpio$pin"
removeFile="/sys/class/gpio/unexport"

if [ -d $file ]
then
	echo $pin > $removeFile
        exit 0
else
	exit 0
fi
