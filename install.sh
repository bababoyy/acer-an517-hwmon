#!/bin/sh
set -eu
version=0.2.1
module=acer-an517-hwmon
if [ "$(id -u)" -ne 0 ]; then
    echo "Run with sudo or pkexec." >&2
    exit 1
fi
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test "$(cat /sys/class/dmi/id/sys_vendor)" = Acer
test "$(cat /sys/class/dmi/id/product_name)" = 'Nitro AN517-41'
command -v dkms >/dev/null
test -d "/lib/modules/$(uname -r)/build"
if [ -e "/usr/src/$module-$version" ] || [ -d "/var/lib/dkms/$module/$version" ]; then
    echo "Version $version already exists; inspect/remove that version before retrying." >&2
    exit 1
fi
install -d "/usr/src/$module-$version"
for file in acer_an517_hwmon.c Makefile dkms.conf; do
    install -m 644 "$here/$file" "/usr/src/$module-$version/"
done
dkms add -m "$module" -v "$version"
dkms build -m "$module" -v "$version" -k "$(uname -r)"
if [ -d /sys/module/acer_an517_hwmon ]; then
    modprobe -r acer_an517_hwmon
fi
for previous in 0.1.0 0.2.0; do
    if [ -d "/var/lib/dkms/$module/$previous" ]; then
        dkms remove -m "$module" -v "$previous" --all
    fi
    rm -rf "/usr/src/$module-$previous"
done
dkms install -m "$module" -v "$version" -k "$(uname -r)"
depmod -a
modprobe acer_an517_hwmon
if ! test -e /sys/bus/wmi/drivers/acer_an517_hwmon/7A4DDFE7-5B5D-40B4-8595-4408E0CC7F56; then
    # WMI device names may carry an instance suffix. Check the actual binding.
    bound=false
    for device in /sys/bus/wmi/drivers/acer_an517_hwmon/7A4DDFE7-*; do
        if [ -L "$device" ]; then bound=true; break; fi
    done
    if [ "$bound" != true ]; then
        echo 'Module loaded but WMI device did not bind. Inspect kernel logs; boot loading was not enabled.' >&2
        exit 1
    fi
fi
install -d /etc/modules-load.d
printf '%s\n' acer_an517_hwmon > /etc/modules-load.d/acer-an517-hwmon.conf
echo 'Fan driver installed. Check: sensors acer_an517-*'
