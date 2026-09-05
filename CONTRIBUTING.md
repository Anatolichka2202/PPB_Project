# Contributing to PPB

Before changing the project, read [`docs/PROJECT_RULES.md`](docs/PROJECT_RULES.md).

## Minimal workflow

1. Create a focused branch (`feature/`, `fix/`, `infra/`, or `release/`).
2. Make the smallest coherent change.
3. Build locally when practical.
4. Open a pull request to `master`.
5. Do not merge while required CI is red.
6. Bump `VERSION` when the delivered user-facing artifact changes.

## Release checklist

- `VERSION` contains a valid Semantic Version;
- tag is exactly `v<VERSION>` (for example `v1.1.0-beta.2`);
- Windows installer CI passes from a clean checkout;
- installer smoke-install passes;
- installed application starts in `--test` mode;
- release notes distinguish software validation from physical hardware validation.

## Local version check

A built PPB executable must report the same version as the root `VERSION` file via:

```text
PPB_GUI_Sandbox.exe --version
```

## Hardware-specific changes

If a change affects CH375, AKIP, PPB communication, timing, or physical I/O, state explicitly whether it was tested on real equipment. Passing mock mode is not a substitute for physical validation.
