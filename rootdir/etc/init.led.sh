#!/system/bin/sh

# prevent boot led from overriding charging led
ledstatus = `cat /sys/class/sec/led/led_pattern`
if [ ledstatus == "6" ]
then
echo 0 > /sys/class/sec/led/led_pattern
fi
