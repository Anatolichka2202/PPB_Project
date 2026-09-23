# PPB 1.1.0-beta.2

Release candidate built from the current `master` state as of 2026-09-23.

Included since the previous published `v1.01-beta` release:
- Windows installer/release infrastructure and application icon;
- SemVer-based version propagation and GitHub Releases updater;
- firmware/host safety fixes merged on 2026-09-07;
- latest FU widget command payload adjustment from commit `88e63179cfc0e128375fa6c1e68edcfb933f6911`.

Validation policy:
- clean Windows build and link;
- NSIS package generation;
- silent installation and runtime dependency checks;
- installed application startup in `--test` mock mode;
- SHA-256 checksum generation.

Passing CI is not a claim of validation on a physical PPB/AKIP/CH375 stand.
