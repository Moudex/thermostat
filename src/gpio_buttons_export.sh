#!/bin/sh
# Ouvre les fichiers spéciaux pour les boutons
# Et activation des interruptions sur fronts descendants
# 16+	26-

bp=16
bm=26

for button in $bp $bm
do
    [ -d "/sys/class/gpio/gpio${button}" ] ||
	echo $button > /sys/class/gpio/export
    [ -w "/sys/class/gpio/gpio${button}/direction" ] && {
	echo in > /sys/class/gpio/gpio${button}/direction
	echo falling > /sys/class/gpio/gpio${button}/edge
    }
done
