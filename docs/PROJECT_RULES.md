# PPB project rules

## 1. Versioning

PPB uses Semantic Versioning: `MAJOR.MINOR.PATCH[-PRERELEASE]`.

The single source of truth is the root `VERSION` file.

Examples:
- `1.1.0-beta.2` — prerelease build;
- `1.1.0` — stable release;
- `1.2.0` — backwards-compatible feature release;
- `2.0.0` — incompatible public behavior/protocol change.

Version bump rules:
- PATCH: bug fixes, packaging fixes, internal refactoring without intended compatibility break;
- MINOR: new backwards-compatible functionality;
- MAJOR: incompatible public behavior, protocol, persistent format, or operator workflow change;
- prerelease identifiers (`alpha.N`, `beta.N`, `rc.N`) are incremented for test releases.

Release tags MUST be exactly `v<VERSION>`, for example `v1.1.0-beta.2`.

## 2. Branches and pull requests

Direct feature work in `master` is prohibited by project convention.

Use short-lived branches:
- `feature/<name>` — new functionality;
- `fix/<name>` — bug fixes;
- `infra/<name>` — build, installer, CI, tooling;
- `release/<version>` — release preparation when needed.

A change is ready to merge when:
1. it has a focused PR;
2. Windows CI is green;
3. relevant smoke/unit tests pass;
4. the version is bumped when the delivered user-visible artifact changes;
5. release notes or migration notes exist for behavior that operators need to know about.

## 3. Commit convention

Preferred commit prefixes:
- `feat:` new functionality;
- `fix:` bug fix;
- `refactor:` internal restructuring;
- `test:` tests only;
- `docs:` documentation only;
- `build:` build system / dependencies;
- `ci:` GitHub Actions;
- `release:` version/release preparation.

Commits should be narrowly scoped. Do not mix unrelated formatting/refactoring with functional changes.

## 4. Build and dependency rules

- CMake is the authoritative build description.
- Supported desktop baseline is Qt 6 for packaged Windows releases.
- CI builds MUST succeed from a clean clone without developer-local library paths.
- Third-party binaries MUST NOT be silently downloaded from unofficial mirrors.
- Proprietary/vendor runtimes such as WCH CH375 binaries are not redistributed unless their redistribution terms are explicitly verified.
- New external dependencies require a reason and a pinned version/revision where reproducibility matters.

## 5. Hardware and test claims

CI without physical PPB/AKIP equipment is a software packaging/smoke validation only.

Do not describe a change as hardware-validated unless it was tested on the corresponding physical stand/device. PR/release notes must distinguish:
- compile/link validation;
- installer/runtime validation;
- mock/test-mode validation;
- physical hardware validation.

## 6. Update policy

Packaged PPB builds check GitHub Releases for newer versions after normal startup.

Rules:
- prerelease builds may receive newer prereleases and stable releases;
- stable builds ignore prereleases;
- draft releases are ignored;
- only SemVer tags are considered;
- Windows updates use the `PPB-<version>-Windows-x86_64.exe` release asset;
- if GitHub provides an asset SHA-256 digest, PPB verifies it before launching the installer;
- update checks are disabled in `--test` mode and can be disabled with `--no-update-check`.

The updater never replaces its own executable in-place. It downloads the verified NSIS installer to a temporary directory, starts it, and exits PPB so Windows can complete the upgrade safely.

## 7. Release procedure

1. Update `VERSION`.
2. Merge a green PR into `master`.
3. Create/push tag `v<VERSION>` from the intended release commit.
4. GitHub Actions rebuilds and smoke-tests the Windows installer.
5. CI publishes the installer and `SHA256SUMS.txt` as a GitHub Release.
6. For prerelease versions, the GitHub Release is marked prerelease automatically.

Never publish a release from an unreviewed branch or from a failed CI run.
