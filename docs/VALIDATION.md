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

Configurable `min_fan_percent` with default 50, explicit negative PWM rejection, and updated packaging/docs. Module compiled with W=1 against the kernel headers above. Shell scripts syntax-checked. Subsequently upgraded from installed 0.2.0 using install.sh and exercised 0.2.1 on hardware. The machine now runs 0.2.1 with the default 50% floor. Low speeds remain untested.

## Outstanding

Suspend/resume, hibernation, real shutdown/reboot, WMI fault injection, low targets and fan restart, other BIOS/kernel versions, and installer removal testing for 0.2.1 remain unverified.

## 0.2.1 live follow-up

See `validation-v0.2.1.json` and `controller-validation-v0.2.1.json` at the repository root.

- Default 50% floor: PWM -1, 127, 256 and modes -1, 3 rejected on both channels; manual entry initializes 255 and repeated enable preserves target 128. Short sample at 50%: CPU 3797 / GPU 4285 RPM; at 70%: CPU 4225 / GPU 4918 RPM. These short samples are not settled calibration measurements.
- Configured floor 70: PWM 178 rejected and 179 accepted/read back on both channels.
- Floor 0: module binds and reports 0; negative PWM remains rejected. No target below 50% was written, so this establishes configuration acceptance only.
- Floor 101: module can load, but device binding is rejected; no acer_an517 hwmon is created.
- Unload from manual control and reload: firmware Auto read back on both channels. Final module parameter restored to 50.
- CoolerControl UI applied CPU Manual 70%. Graceful daemon stop returned both channels to Auto; starting it reapplied saved CPU Manual 70%. SIGKILL test observed manual CPU target 179 throughout ten samples at 0.5-second intervals; systemd restarted the daemon. This is not an independent failsafe. GPU remained Auto throughout this controller lifecycle test.
- Afterwards the UI was set to Unmanaged for CPU, leaving both channels Auto and CoolerControl running.

Tests were supervised at low load with an 85°C CPU abort check in the direct control test. Temperature threshold was a test abort condition, not a validated thermal safety limit. No full-load thermal qualification was performed.
