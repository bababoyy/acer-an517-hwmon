# acer-an517-hwmon

Experimental Linux hwmon driver for **Acer Nitro AN517-41** CPU/GPU fan monitoring and control through Acer Gaming WMI. Works with standard hwmon clients such as CoolerControl. No raw EC or physical memory access.

**This is a hardware-tested prototype, not a broadly validated driver.** Tested hardware: Acer Nitro AN517-41, BIOS V1.14, Linux Mint 22.3, kernel 6.8.0-147-generic (x86_64). Other models are rejected. Fan control is enabled only on BIOS V1.14 after successful mode/target queries; other BIOS versions have not been tested.

## Validation status

Source version **0.2.1** adds a configurable manual speed floor. It is build-checked; the new parameter has not been tested on hardware. The included hardware results are from **0.2.0**, which used a fixed 50% floor. This repository does not claim that lower speeds or fan stop work.

On 0.2.0, RPM reads, 50%/70% manual targets, full speed, firmware Auto, invalid input rejection, repeated manual enable, and Auto restoration on module unload were exercised. CoolerControl 5.0.1 detected both fans and successfully controlled each at 70%. See [test results](docs/VALIDATION.md).

## Install

Requirements: DKMS, make, a C compiler, and headers matching the running kernel. On Linux Mint/Ubuntu:

```sh
sudo apt install dkms build-essential linux-headers-$(uname -r)
sudo sh install.sh
```

The installer builds before replacing a loaded older version, installs source under `/usr/src/acer-an517-hwmon-0.2.1`, and enables module loading at boot. Stop fan-control software before installing/upgrading/uninstalling. Do not use this module alongside another driver controlling the same Acer Gaming WMI device. Secure Boot may require enrolling the distribution's DKMS signing key. Kernel API compatibility on other kernel releases is unverified.

```sh
sudo sh uninstall.sh
```

The uninstaller removes this project's known versions (0.1.0, 0.2.0, 0.2.1). It leaves any administrator-created modprobe options file for you to review/remove. Install/uninstall is not transactional: retain error output if a DKMS step fails.

## Manual target floor

Default: **50%**. This is a conservative project policy, not a measured physical minimum or a guarantee of sufficient cooling.

To configure it, create `/etc/modprobe.d/acer-an517-hwmon.conf`:

```conf
options acer_an517_hwmon min_fan_percent=50
```

Valid values are 0–100, applying to both fans. `0` removes the floor and permits a zero target. **Values below 50%, fan stop, and restart from a stopped fan are untested.** Firmware may clamp or interpret targets differently. The percentage is a firmware target, not a percentage of maximum RPM.

The parameter is selected when loading the module and is read-only while loaded. An already loaded module must be unloaded/reloaded for changes to take effect; first stop your controller and return both fans to firmware Auto. Changing the file alone does not change the running driver. Manual entry still initializes the target to 100%.

## hwmon interface

Find the hwmon directory whose `name` is `acer_an517`; its numeric index is not stable.

| Attribute | Meaning |
| --- | --- |
| `fan1_input`, `fan2_input` | CPU/GPU fan RPM; read-only, cached up to one second |
| `fan1_label`, `fan2_label` | CPU Fan / GPU Fan |
| `pwm1`, `pwm2` | Firmware target mapped to 0–255, constrained by the configured floor |
| `pwm1_enable`, `pwm2_enable` | 2 = firmware Auto, 1 = manual, 0 = firmware full speed |

Targets are rounded to integer firmware percentages; readback may differ by one PWM unit. Writing a target alone does not enter manual mode. Re-enabling an existing valid manual session preserves its target. In Auto, the PWM value can show a stale manual target; it does not describe current automatic fan speed.

CoolerControl can consume these attributes without a custom plugin. **Unmanaged** restored firmware Auto in our integration test. Configure manual targets/curves at or above the selected floor. Do not assume the application's calibration wizard is compatible: it has not been tested. Temperature inputs come from existing drivers such as k10temp/amdgpu; this module supplies no temperature sensor.

## Failure handling and limitations

Firmware status and write readback are checked. On a control error, the driver attempts to return channels it owns to Auto and logs restoration failures. Module removal, shutdown/reboot notification and suspend callbacks also attempt Auto restoration for owned channels. Resume does not restore manual settings.

**There is no userspace heartbeat or command timeout. If the controller crashes, the last manual target can remain active.** A minimum target alone cannot ensure adequate cooling under load. Do not leave unvalidated manual settings unattended.

Suspend/hibernate, physical reboot, controller crash, WMI fault injection and low-speed operation have not been tested. Callback code is not evidence those scenarios work. The driver does not take ownership of manual settings established by another application before it loaded.

## Research and contributing

See [the WMI protocol notes](docs/PROTOCOL.md), [validation evidence](docs/VALIDATION.md), and [the roadmap](docs/ROADMAP.md). Reports should include model, BIOS, kernel, source version, module parameters, controller version and reproducible observations. Review logs before sharing; do not attach serial numbers, UUIDs or full ACPI dumps by default.

This is an independent experimental project, not an Acer, ASense, CoolerControl or upstream Linux release. It exposes a narrow standard interface rather than providing a new desktop controller.

## License and references

GPL-2.0-only; see [LICENSE](LICENSE). Standard hwmon/WMI integration was informed by Linux documentation and acer-wmi. Acer fan-control projects, including ASense and DivAcerManager, were research references; AN517-41 payloads were checked against local firmware tables and hardware readback. See the protocol document for scope and sources. No upstream acceptance or endorsement is implied.
