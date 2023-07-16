
#!/bin/sh

if [ $# -eq 1 ]
then
        pin=$1
else
        printf "Usage: gpioExport gpioNumber\n"
        exit 1
fi

file="/sys/class/gpio/gpio$pin"
createFile="/sys/class/gpio/export"

if [ -d $file ]
then
        exit 0
else
	echo $pin > $createFile
	exit 0
fi
