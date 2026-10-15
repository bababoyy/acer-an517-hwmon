# Changelog

## 0.2.1 — experimental source snapshot

- Add load-time `min_fan_percent` (default 50; 0 disables the floor).
- Explicitly reject negative PWM values with an unsigned configured threshold.
- Document protocol, validation scope, installation and limitations in English.
- Follow-up hardware checks cover floor configuration, invalid inputs, module unload and CoolerControl stop/crash behavior; see docs/VALIDATION.md.

## 0.2.0 — local hardware prototype

- Add WMI manual/full/Auto control with readback and a fixed 50% minimum.
- Exercise both fans through sysfs and CoolerControl on AN517-41 / BIOS V1.14.

## 0.1.0 — local prototype

- Read-only CPU/GPU fan RPM via hwmon.
