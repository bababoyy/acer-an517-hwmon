# Experimental release roadmap

1. Validate 0.2.1 on hardware at the default floor, including invalid module parameters and negative PWM inputs. Do not conflate a successful build with live validation.
2. Verify suspend/resume and controller shutdown/crash behavior; design a documented recovery policy before unattended manual curves.
3. Investigate lower targets only in a separate supervised experiment with temperatures, RPM and reliable Auto recovery recorded.
4. Prepare an ASense compatibility report with protocol observations and exact test scope. Check existing reports before opening an issue; ASense itself has not been validated by these tests.
5. Consider upstream acer-wmi/hwmon integration after review of ownership, lifecycle and thermal behavior. A standalone experimental module is not an upstream-ready patch.

Publication scope: source + installation/removal + protocol notes + historical test evidence. No automatic hardware calibration, support for additional models, or safety guarantees.
