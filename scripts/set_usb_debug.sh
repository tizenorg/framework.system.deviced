#!/bin/sh

case "$1" in

"--debug")
	/usr/bin/vconftool set -t int db/usb/sel_mode "2" -f
	/usr/bin/vconftool set -t bool db/setting/debug_mode "1" -f
	/usr/bin/vconftool set -t int db/setting/lcd_backlight_normal "600" -f
	echo "The backlight time of the display is set to 10 minutes"
	/usr/bin/vconftool set -t bool db/setting/brightness_automatic "1" -f
	echo "Brightness is set automatic"
	;;

"--mtp" | "--sshoff" | "--unset")
	/usr/bin/vconftool set -t int db/usb/sel_mode "1" -f
	/usr/bin/vconftool set -t bool db/setting/debug_mode "0" -f
	;;

"--mtp-sdb" | "--set")
	/usr/bin/vconftool set -t int db/usb/sel_mode "2" -f
	/usr/bin/vconftool set -t bool db/setting/debug_mode "1" -f
	;;

"--mtp-sdb-diag")
	/usr/bin/vconftool set -t int db/usb/sel_mode "3" -f
	/usr/bin/vconftool set -t bool db/setting/debug_mode "1" -f
	;;

"--rndis-tethering")
	/usr/bin/vconftool set -t int db/usb/sel_mode "4" -f
	;;

"--rndis" | "--sshon")
	/usr/bin/vconftool set -t int db/usb/sel_mode "5" -f
	/usr/bin/vconftool set -t bool db/setting/debug_mode "0" -f
	;;

"--rndis-sdb")
	/usr/bin/vconftool set -t int db/usb/sel_mode "6" -f
	/usr/bin/vconftool set -t bool db/setting/debug_mode "1" -f
	;;

"--rndis-diag")
	/usr/bin/vconftool set -t int db/usb/sel_mode "8" -f
	/usr/bin/vconftool set -t bool db/setting/debug_mode "0" -f
	;;

"--help")
	echo "set_usb_debug.sh: usage:"
	echo "    --help           This message "
	echo "    --set            Load Debug mode"
	echo "    --unset          Unload Debug mode"
	echo "    --debug          Load debug mode with 10 minutes backlight time"
	echo "                     and automatic brightness"
	echo "    --sshon          Load SSH mode"
	echo "    --sshoff         Unload SSH mode"
	echo "    --mtp            Load mtp only"
	echo "    --mtp-sdb        Load mtp and sdb"
	echo "    --mtp-sdb-diag   Load mtp, sdb, and diag"
	echo "                     If diag is not supported, mtp is loaded"
	echo "    --rndis          Load rndis only"
	echo "    --rndis-diag     Load rndis and diag"
	echo "                     If diag is not supported, mtp is loaded"
	;;

*)
	echo "Wrong parameters. Please use option --help to check options "
	;;

esac
