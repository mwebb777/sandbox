#!/bin/bash
# Quick diagnostic for Raspberry Pi 5 PWM setup

echo "=== PWM diagnostic ==="
echo

echo "1. Checking for pwm chips:"
ls -l /sys/class/pwm/ 2>/dev/null || echo "  No /sys/class/pwm found – is the overlay loaded?"

echo
echo "2. Current dtoverlay lines in config:"
grep -E "pwm|dtoverlay" /boot/firmware/config.txt 2>/dev/null || \
grep -E "pwm|dtoverlay" /boot/config.txt 2>/dev/null || \
echo "  Could not read config.txt"

echo
echo "3. Suggested addition to /boot/firmware/config.txt:"
echo "   dtoverlay=pwm-2chan"
echo "   (then reboot)"

echo
echo "4. After overlay is active you should see something like:"
echo "   /sys/class/pwm/pwmchip0"
echo "   Channels 0 and 1 correspond to GPIO 12 and 13 (BCM)"

echo
echo "5. Test export (as root):"
echo "   echo 0 > /sys/class/pwm/pwmchip0/export"
echo "   ls /sys/class/pwm/pwmchip0/pwm0"
echo
