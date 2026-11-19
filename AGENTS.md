# Repository Guidelines

## Project structure

Mu-Silicium builds Project Mu UEFI firmware for ARM64 devices. Put board-specific code in `Platforms/<vendor>/<codename>Pkg/` and shared SoC drivers and libraries in `Silicon/<vendor>/`. For example, `Platforms/Samsung/a10Pkg/a10.dsc` selects components for the Galaxy A10, and `a10.fdf` defines its firmware image layout.

The build script pairs each device package with `Resources/Configs/<codename>.toml`. Device trees live in `Resources/DTBs/` and device photos in `Resources/Pictures/`. `Mu_Basecore/` and `Common/` supply Project Mu dependencies. Update `Status.md` when device support changes. The batch build reads device codenames and model counts from its active entries.

## Setup and build

Run these commands from the repository root. See [Building.md](Building.md) for Windows setup.

- `git submodule update --init --recursive` fetches dependencies after cloning.
- `./setup_env.sh -p apt` installs the Linux toolchain and Python dependencies on Debian or Ubuntu. The script also accepts `dnf` and `pacman`.
- `python3 build_uefi.py -d a10 -r DEBUG` builds debug firmware for the Galaxy A10. Use a codename from `Status.md`. Omit `-r DEBUG` for a release build, or add `-m 1` to select the second model on a multi-model target.

## Coding style

Match the surrounding EDK II C code. Use two-space indentation, types such as `EFI_STATUS`, and PascalCase function names. Use four spaces in Python and snake_case for helpers in `build_uefi.py`. Keep the codename consistent across the package directory, DSC, FDF, and TOML filenames.

## Testing

Build the affected device and test the changed behavior on hardware. For shared changes, `./build_active_devices.sh -r DEBUG` reproduces the CI build across active devices and models. CI sets no coverage threshold. OpenSSL host tests use EDK II's UnitTestFrameworkPkg through `Silicon/Silicium/OpensslPkg/Test/OpensslPkgHostUnitTest.dsc`.

## Commits and pull requests

Write a short, action-oriented commit subject. Recent history often prefixes it with a device or component, as in `onyx: Update Port`.

Choose **Generic Changes** to describe what changed, why, and how you tested it. For **Device support**, give the device name, codename, and SoC. Attach a photo showing the device running a debug build at the Boot Device Selection (BDS) stage and complete the template checklist.
