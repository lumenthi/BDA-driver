#!/bin/bash

trap ctrl_c INT

function ctrl_c()
{
	echo "Trapped CTRL-C"
	pkill cat
}

cat /dev/usb/skel0 1>/dev/null &
jstest /dev/input/js0
