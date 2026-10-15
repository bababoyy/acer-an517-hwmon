# Validation ledger

Hardware: Acer Nitro AN517-41, BIOS V1.14; Linux Mint 22.3; kernel 6.8.0-147-generic, x86_64. Recorded 2026-10-15. One machine; short functional checks, not thermal qualification.

## Version 0.2.0 — hardware checks

| Target | CPU RPM | GPU RPM |
| --- | --- | --- |
| 50% | 3703 | 4109 |
| 70% | 4166 | 4615 |
| Full speed, separate initial trial | 5000 | 5769 |

Invalid PWM 127/256 and enable value 3 were rejected. Repeated manual enable preserved a valid target. Firmware Auto and full-mode readback passed. Unloading the module during manual control restored Auto, verified after reload.

CoolerControl 5.0.1 detected both fans. Each was set independently to 70% through its UI; sysfs showed PWM 179 and manual mode. Both were then set to Unmanaged, resulting in `pwm1_enable=2` and `pwm2_enable=2`; final sample 2290/2439 RPM. No custom curve or calibration wizard was used.

The root `validation.json`, `control-validation.json`, and `coolercontrol-validation.json` are historical 0.2.0 evidence, not tests of 0.2.1. The hwmon index in them is an incidental runtime index.

## Version 0.2.1 — source preparation

Configurable `min_fan_percent` with default 50, explicit negative PWM rejection, and updated packaging/docs. Module compiled with W=1 against the kernel headers above. Shell scripts syntax-checked. No 0.2.1 install, low-speed test or hardware validation has been performed. The running machine was left on its previously installed 0.2.0 driver.

## Outstanding

Suspend/resume, hibernation, real shutdown/reboot, controller failure, WMI fault injection, low targets and fan restart, other BIOS/kernel versions, and installer upgrade/removal testing for 0.2.1 remain unverified.
