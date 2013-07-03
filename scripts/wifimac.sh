#!/system/bin/sh

# Hardcoded sources and destinations based on apexqtmo. Optional
# commandline parameter to override MACSOURCE in order to specify
# custom MAC address.

# Expected MAC address format: XX:XX:XX:XX:XX:XX, whether it be from
# MACSOURCE or from commandline.

# Edit the following four variables for other devices as needed.
NVSOURCE="/system/etc/firmware/wlan/prima/WCNSS_qcom_wlan_nv.bin_"
SYM="/system/etc/firmware/wlan/prima/WCNSS_qcom_wlan_nv.bin"
MACSOURCE="/efs/wifi/.mac.info"
DEST="/persist/WCNSS_qcom_wlan_nv.bin"

MACADDR=""

# Make sure the script is running as root
if [ `whoami` != "root" ]; then
	echo "Must run as root"
	return 0
fi

if [ ! -f $NVSOURCE ]; then
	echo "Source file $NVSOURCE does not exist!!"
	return 0
fi

# Run from recovery, there is no /persist directory. Create it, mount
# /persist, and then remember to umount it when done.
UMOUNTPERSIST=false
if [ ! -d /persist ]; then
	mkdir /persist
	mount /dev/block/platform/msm_sdcc.1/by-name/persist /persist
	mkdir /efs
	mount /dev/block/platform/msm_sdcc.1/by-name/efs /efs
	UMOUNTPERSIST=true
fi

# If the user wishes to insert a custom mac address on the commandline
# accept it, and keep going. If the user does something that makes no
# sense, call them a retard and exit.
if [ $# == 1 ]; then
	MACADDR=$1
elif [ $# == 0 ]; then
	MACADDR=`cat $MACSOURCE`
else
	echo "User is retarded."
	return 1
fi

# convert to lower case
MACADDR=`echo $MACADDR | tr '[:upper:]' '[:lower:]'`

# get the bytes individually. Edit for alternative input format
# if needed for other devices.
B1=`echo $MACADDR | cut -d\: -f1`
B2=`echo $MACADDR | cut -d\: -f2`
B3=`echo $MACADDR | cut -d\: -f3`
B4=`echo $MACADDR | cut -d\: -f4`
B5=`echo $MACADDR | cut -d\: -f5`
B6_1=`echo $MACADDR | cut -d\: -f6`

# Now prima calls for 4 unique mac addresses. What it typically does
# is it takes the last byte and increments it by 0-->3 to generate
# the four. Rather than complicated math on ascii representations of
# hexidecimal digits, we are going to ONLY manipulate the final digit
# not the entire final byte. We do this by looking up the position of
# the final digit in a lookup string, and then taking the next three
# characters. Because we only need 4 out of 16, this will suffice.
B6a=`echo $B6_1 | cut -c1`
B6b=`echo $B6_1 | cut -c2`
LOOKUP="0123456789abcdef012"

IDX=`expr index "$LOOKUP" "$B6b"`
B6_2=$B6a`echo $LOOKUP | cut -c$((IDX+1))`
B6_3=$B6a`echo $LOOKUP | cut -c$((IDX+2))`
B6_4=$B6a`echo $LOOKUP | cut -c$((IDX+3))`

# The actual edit. Don't trust recovery's busybox to have "-R",
# use freshly installed busybox on /system/xbin instead.
/system/xbin/busybox hexdump -Cv $NVSOURCE | sed -e "s/^00000000  \(.*\)00 00 00 00 00 00  |/00000000  \1$B1 $B2 $B3 $B4 $B5 $B6_1  |/" | sed -e "s/^00000010  .*  |/00000010  $B1 $B2 $B3 $B4 $B5 $B6_2 $B1 $B2  $B3 $B4 $B5 $B6_3 $B1 $B2 $B3 $B4  |/" | sed -e "s/^00000020  00 03\(.*\)/00000020  $B5 $B6_4\1/" | /system/xbin/busybox hexdump -R > $DEST

if ( $UMOUNTPERSIST ); then
	# If we aren't running from recovery, the link should
	# already exist, plus, /system will be mounted RO.
	# Symlink to target file from location expected by driver.
	busybox ln -sf $DEST $SYM
	umount /persist
	umount /efs
	rmdir /persist
	rmdir /efs
fi

