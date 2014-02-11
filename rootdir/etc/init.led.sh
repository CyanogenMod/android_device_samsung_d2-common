#!/system/bin/sh

# prevent boot led from overriding charging led
LEDSTATUS = `cat /sys/class/sec/led/led_pattern`
if [ $LEDSTATUS = "6" ]; then
    echo 0 > /sys/class/sec/led/led_pattern
fi
