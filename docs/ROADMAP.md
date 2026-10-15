# Experimental release roadmap

1. Completed: 0.2.1 default/70% floor, negative inputs, invalid floor binding, unload restoration and CoolerControl CPU stop/crash behavior. Remaining: parameter persistence across a real boot, installer removal, full-speed regression on 0.2.1.
2. Verify suspend/resume and address the now-observed controller crash limitation; design a documented recovery policy before unattended manual curves.
3. Investigate lower targets only in a separate supervised experiment with temperatures, RPM and reliable Auto recovery recorded.
4. Prepare an ASense compatibility report with protocol observations and exact test scope. Check existing reports before opening an issue; ASense itself has not been validated by these tests.
5. Consider upstream acer-wmi/hwmon integration after review of ownership, lifecycle and thermal behavior. A standalone experimental module is not an upstream-ready patch.

Publication scope: source + installation/removal + protocol notes + historical test evidence. No automatic hardware calibration, support for additional models, or safety guarantees.
