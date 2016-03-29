#!/bin/sh
# Ouvre les fichiers spéciaux pour les relais

chauff=20
congel=21

for button in $chauff $congel
do
    [ -d "/sys/class/gpio${button}" ] ||
	echo $button > /sys/class/gpio/export
    [ -w "/sys/class/gpio/gpio${button}/direction" ] &&
	echo out > /sys/class/gpio/gpio${button}/direction
done
