#!/bin/sh
set -eu
if [ "$(id -u)" -ne 0 ]; then
    echo "Run with sudo or pkexec." >&2
    exit 1
fi
if [ -d /sys/module/acer_an517_hwmon ]; then
    modprobe -r acer_an517_hwmon
fi
rm -f /etc/modules-load.d/acer-an517-hwmon.conf
for version in 0.1.0 0.2.0 0.2.1; do
    if [ -d "/var/lib/dkms/acer-an517-hwmon/$version" ]; then
        dkms remove -m acer-an517-hwmon -v "$version" --all
    fi
    rm -rf "/usr/src/acer-an517-hwmon-$version"
done
depmod -a
echo 'Fan driver removed. Administrator-created modprobe options were preserved.'
