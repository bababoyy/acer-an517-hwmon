# AN517-41 / BIOS V1.14 protocol observations

These observations come from local ACPI table inspection and fixed-method WMI calls on one machine. They are not an Acer specification or a claim of compatibility with other models. Source uses WMI, not direct EC access.

Gaming GUID: `7A4DDFE7-5B5D-40B4-8595-4408E0CC7F56`, ACPI object BH, WMI instance 0. Integers/payloads below use little-endian layout. Firmware status is the low byte; zero indicates success in the tested calls.

| Method | Input | Result interpretation |
| --- | --- | --- |
| 0x05, RPM | 8 bytes: CPU `[1,2,0,0,0,0,0,0]`, GPU `[1,6,0,0,0,0,0,0]` | status byte 0; RPM little-endian bytes 1–2 |
| 0x0F, get mode | u32 bitmap: CPU 1, GPU 8 | observed 8-byte response; CPU mode bits 8–9, GPU bits 14–15 |
| 0x0E, set mode | u64 bitmap 1/8 OR mode shifted 16 (CPU) / 22 (GPU) | observed 8-byte response; status then get-mode readback |
| 0x11, get target | u32 fan ID: CPU 1, GPU 4 | observed 8-byte response; status byte 0, integer percent byte 1 |
| 0x10, set target | u64 fan ID 1/4 OR percent shifted 8 | observed **4-byte** response; status then get-target readback |

Firmware modes: 1 = Auto, 2 = full/Turbo, 3 = custom/manual. These differ from hwmon's `pwm_enable` values; the driver maps between them. Reply validation checks the bytes needed by each operation (RPM >=3, target getter >=2, target setter >=4, modes >=8), not a universal eight-byte requirement.

An early prototype incorrectly required eight bytes for the target-set reply. Method-specific validation resolved that failure. A suspected DivAcerManager packet-concatenation bug was not substantiated and should not be reported as a confirmed bug.

The 50% floor is project policy. Firmware tables suggest a 0–100 target range, but acceptance and safe mechanical behavior below 50% have not been established. RPM is not linear in the target percentage. No raw ACPI tables or machine identifiers are included here.

References:

- Linux hwmon API: https://docs.kernel.org/hwmon/hwmon-kernel-api.html
- Linux hwmon sysfs: https://docs.kernel.org/hwmon/sysfs-interface.html
- Upstream Acer driver: https://github.com/torvalds/linux/blob/master/drivers/platform/x86/acer-wmi.c
- ASense: https://github.com/fladirm/asense
- CoolerControl devices: https://docs.coolercontrol.org/devices/

Do not run ASense's transport and this module concurrently against the same device. Supporting a model here does not establish that ASense itself was tested on it.
