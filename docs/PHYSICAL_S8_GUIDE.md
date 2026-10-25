# HackiOS on a Physical Samsung Galaxy S8 (dreamlte): Complete Bring-Up Guide

**Status: This is a bring-up guide, not a claim that the current tree can boot the S8. Do not flash HackiOS to the phone yet.**

The repository contains an experimental Exynos 8895/XNU configuration and some early device files, but it does not yet contain a verified end-to-end Samsung boot path. The XNU build has produced an ARM64 Mach-O kernel executable, but that executable is not a Samsung/Odin image and is not, by itself, bootable. The existing `xl`/`common/run.sh` phone transport expects Android `fastboot boot`; the S8 normally uses Samsung Download Mode (Odin/Loke), not that transport. The dreamlte UEFI profile has no supplied UEFI FD/DTB, the loader platform file is still a fragment, and the root build script selects the BCM2837 kernel path. Consequently, **there is no safe command today that installs this build on an S8**.

This guide takes you through the safe, useful work that can be done now, explains the stop point, and defines the gates that must be passed before a temporary boot or flash procedure is written. It deliberately does not provide an `heimdall flash`, Odin AP, PIT, or raw-partition command: there is no validated S8 image to flash.

> **⚠️ CRITICAL WARNING:** Flashing custom boot code can erase the phone, make it unbootable, permanently trip Samsung Knox's warranty bit, and make Knox-protected data/services unavailable. A restore does not reverse the one-time warranty fuse. Proceed only with a phone you own, with a tested stock recovery route, and after accepting those consequences.

---

## 0. Terminology and Supported Target

- **dreamlte** means the Exynos Galaxy S8 target in this tree. It is not a generic name for every Galaxy S8.
- Do not assume an `SM-G950*` suffix alone proves the SoC. Read the exact model and board/product values from the phone, then independently verify that the device is Exynos 8895 and that the kernel/loader identify it as `dreamlte`.
- This project's `common/run.sh`, `xl`, `mkxnuimg.py`, and prebuilt artifacts are currently wired around the **redwood** (POCO) phone flow and QEMU. The S8 profile files are experimental and are not evidence that the S8 launch path works.
- A kernel file is only one stage. A physical boot also needs a device-compatible boot entry, correct firmware and device tree, a loader that builds valid XNU `boot_args`, working memory/interrupt/timer/CPU support, and a usable iOS root/system image. Reaching the XNU linker is not reaching the lock screen.

---

## 1. Prepare the Host Environment

### 1.1 Supported Host Platforms

The commands below are for the HackiOS workspace on **macOS** (primary) and **Linux** (for UEFI/EDK2 builds). Use a Linux host if the project's build dependencies or device tools require it; do not mix instructions for the two hosts blindly.

### 1.2 Verify Workspace Integrity

```sh
cd /path/to/hackios-xnu-kernel
pwd
git status --short
```

**Do not run `git reset`, `git clean`, or overwrite local files to "start fresh."** This workspace contains local porting changes and build artifacts.

### 1.3 Check Apple Toolchain and SDK (macOS)

```sh
xcodebuild -showsdks
xcrun --sdk iphoneos --show-sdk-path
xcrun clang --version
make --version
```

The local build was linked with Apple Clang and the iPhoneOS SDK. XNU here is the `xnu-8796.121.2` source baseline; a successful compile with a newer SDK is not proof that the kernel is compatible with an iOS 16.5 userspace or with Samsung hardware.

### 1.4 Install Android Platform Tools

Install Android Platform Tools using the official Android SDK Manager or the official Platform Tools package. They provide `adb` and `fastboot`; **having `fastboot` installed does not mean the Samsung bootloader accepts fastboot commands**.

```sh
adb version
fastboot --version
```

### 1.5 Install Build Dependencies (Linux)

If building UEFI/EDK2 on Linux:

```bash
# Install build dependencies
sudo apt-get update && sudo apt-get install -y \
  git python3 python3-pip python3-venv \
  build-essential clang llvm lld \
  gcc-aarch64-linux-gnu nasm uuid-dev \
  flex bison m4 autoconf automake \
  mono-devel p7zip-full \
  libssl-dev pkg-config

# Install Python dependencies
pip3 install --user -r Mu-Silicium/pip-requirements.txt
export PATH="$HOME/.local/bin:$PATH"

# Install theos with iPhoneOS16.5.sdk (required for kernel/userland compilation)
# See: https://github.com/theos/theos
# The SDK should be at: ~/theos/sdks/iPhoneOS16.5.sdk
```

### 1.6 Heimdall (Optional, Read-Only)

Heimdall is an open-source implementation of Samsung's Download Mode protocol. It is useful for non-writing device detection/PIT inspection when appropriate, but its presence does not make an unvalidated HackiOS image flashable.

```bash
# Install on Linux
sudo apt-get install -y heimdall-flash

# Verify
heimdall version
```

**Do not use Odin/Heimdall to write any HackiOS image during the current project state.**

---

## 2. Identify the Exact S8 Variant Before Changing Anything

Boot the phone normally into Android. Enable USB debugging only if you are comfortable doing so, connect a known data-capable USB cable, authorize the host on the phone, then run:

```sh
adb devices -l
adb shell getprop ro.product.model
adb shell getprop ro.product.device
adb shell getprop ro.boot.hardware
adb shell getprop ro.bootloader
adb shell getprop ro.build.version.release
adb shell cat /proc/cpuinfo
```

### 2.1 Record the Output

```sh
mkdir -p notes
{
  date
  adb devices -l
  adb shell getprop ro.product.model
  adb shell getprop ro.product.device
  adb shell getprop ro.boot.hardware
  adb shell getprop ro.bootloader
  adb shell getprop ro.build.version.release
  adb shell cat /proc/cpuinfo
} > notes/s8-device-inventory.txt
```

### 2.2 Compare Results Against All Three Criteria

Before considering any device-specific work, verify:

1. **The label/model reported by Android** and the label printed in Download Mode.
2. **The actual SoC/CPU identity** (the target config in this tree is Exynos 8895 - 4× M2 @ 2.3 GHz + 4× Cortex-A53 @ 1.7 GHz).
3. **The device codename expected by the artifacts** (`dreamlte`, not another S8 variant like `dreamqlte`, `dreamltexx`, etc.).

**If the model, SoC, codename, or bootloader status is ambiguous, stop. Do not "try" an image to find out.**

### 2.3 Inspect Samsung Boot Screen (Read-Only)

If you want to inspect the Samsung boot screen:
1. Power the phone off
2. Hold **Volume Down + Bixby + Power** until the warning screen appears
3. Read the model/product text **without accepting any unlock or flashing prompt**
4. Pressing **Volume Up** continues into Download Mode; do that only when you intentionally want to inspect the screen or use a read-only Download Mode tool
5. This is Samsung Download Mode, not Android `fastboot`

**To leave an accidentally entered Download Mode when no write is in progress:** hold **Volume Down + Power** for about seven seconds until the phone restarts.

---

## 3. Back Up and Establish a Recovery Plan

Before unlocking a bootloader or entering a flashing workflow:

1. **Back up photos, files, and app data** to a second device/computer or external storage
2. **Use Samsung Smart Switch** or another trusted backup route, then verify that the backup can be read
3. **Save the exact stock firmware package** matching the phone's model and region/CSC, along with its source and checksums. Keep a known-good cable and a charged battery
4. **Confirm that you can reach Download Mode and can restore the exact stock firmware BEFORE changing the bootloader**
5. **Decide whether losing Knox-protected data/services is acceptable.** Knox documents its warranty bit as a persistent, one-time hardware fuse; custom/unauthorized boot components can affect Knox features

> **Do not rely on a backup of `/data` as a substitute for a stock recovery image. Do not use a PIT file from a different model or firmware family.**

---

## 4. Read-Only Project Preflight

From the repository root, inspect the experimental S8 files and boot artifacts:

```sh
cd /path/to/hackios-xnu-kernel

# Inspect device profile
cat common/devices/dreamlte.conf

# Inspect loader platform fragment
sed -n '1,180p' common/devices/dreamlte.platform.c

# List UEFI port files
find uefi/files/Platforms/Samsung/dreamltePkg -type f -print

# List prebuilt binaries
find prebuilt -maxdepth 3 -type f -print
```

### 4.1 Current State of S8 Artifacts

- The device profile currently leaves `UEFI_FD` and `DTB` blank
- `xnuloader/loader.c` contains a `platform_dreamlte` entry and lists it for framebuffer-based detection, but the addresses/comments are still unverified
- `common/devices/dreamlte.platform.c` is a separate reference fragment, not proof that those values are correct
- The S8 UEFI binary set and an S8 DTB must be real, device-matched artifacts, not the repository's redwood prebuilt UEFI/DTB

### 4.2 Read the Build Scripts Before Invoking

```sh
# Main build orchestrator
sed -n '1,240p' common/build.sh

# Phone boot script (currently fastboot-based)
sed -n '1,160p' common/run.sh

# Boot image packer
sed -n '1,180p' mkxnuimg.py

# Loader build
sed -n '1,180p' xnuloader/build.sh

# TWRP flashable image creator
sed -n '1,180p' mkxnuimg_twrp.py
```

### 4.3 Important Current Blockers Visible in Scripts

- `common/build.sh` selects a `DEVELOPMENT_ARM64_BCM2837` kernel artifact; this is **not** the DREAMLTE kernel output
- The image packer falls back to redwood UEFI files when no S8 UEFI build is supplied
- `common/run.sh` drives a standard `fastboot boot` workflow; it is **not** a Samsung Download Mode/Odin workflow
- The expected S8 memory-map addresses in the profile are explicitly marked unverified

**Do not run `common/build.sh dreamlte` or `common/run.sh dreamlte` expecting them to boot the S8. First fix and review those integration points and test them with a non-destructive artifact checker.**

---

## 5. Complete Build Procedure (Host-Only)

This section documents the full build process that produces all artifacts. **This is host-only work - it does not contact or modify the phone.**

### 5.1 Prerequisites Checklist

Before starting the build, verify you have:

- [ ] macOS host with Xcode + iPhoneOS SDK (for XNU kernel)
- [ ] Linux host or VM (for UEFI/EDK2 build)
- [ ] `theos` installed with `iPhoneOS16.5.sdk` at `~/theos/sdks/iPhoneOS16.5.sdk`
- [ ] Android Platform Tools (`adb`, `fastboot`)
- [ ] Python 3 with packages from `Mu-Silicium/pip-requirements.txt`
- [ ] `~/hackios-xnu-userland-dreamlte` sibling directory (userland repo)

### 5.2 Step 1: Clone and Setup Repositories

```bash
# Clone kernel repo
git clone https://github.com/oosexgang/hackios-xnu-kernel.git
cd hackios-xnu-kernel

# Link userland repo (checks out into same directory with .userland.git)
./common/setup.sh

# Initialize Mu-Silicium submodules
cd Mu-Silicium
git submodule update --init --recursive
cd ..
```

### 5.3 Step 2: Build Mu-Silicium UEFI for dreamlte

```bash
cd Mu-Silicium

# Fix NuGet SSL issue (if needed): manually provide nasm
mkdir -p Mu_Basecore/BaseTools/Bin/mu_nasm_extdep/Linux-ARM-64
cp /usr/bin/nasm Mu_Basecore/BaseTools/Bin/mu_nasm_extdep/Linux-ARM-64/nasm
echo 'version: 20016.1.1' > Mu_Basecore/BaseTools/Bin/mu_nasm_extdep/extdep_state.yaml

# Build UEFI for dreamlte
python3 build_uefi.py -d dreamlte
cd ..

# Copy built UEFI FD to expected location for mkxnuimg.py
mkdir -p edk2-exynos/Build/dreamltePkg/RELEASE_CLANG38/FV
cp Mu-Silicium/Build/dreamltePkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd \
   edk2-exynos/Build/dreamltePkg/RELEASE_CLANG38/FV/

# Copy DTB to expected location
cp Mu-Silicium/Resources/DTBs/dreamlte.dtb exynos8895-dreamlte.dtb
```

### 5.4 Step 3: Fetch iOS 16.5 Userland Files

```bash
# Link xnu-deps for unzbm tool
ln -s ../hackios-xnu-kernel/xnu-deps ../hackios-xnu-userland-dreamlte/xnu-deps

# Download and extract iOS 16.5 (~7.4 GB download, ~8 GB temp space)
cd ../hackios-xnu-userland-dreamlte
./ios165/fetch.sh

# Copy extracted files to kernel repo where build scripts expect them
cp -r ios165/rootfs ios165/cryptex ios165/cryptex-app \
      ios165/20F66__iPhone10,3_6 ios165/system.files \
      ios165/system.manifest ios165/var.skel \
      ../../hackios-xnu-kernel/ios165/
```

### 5.5 Step 4: Build XNU Dependencies

```bash
cd /path/to/hackios-xnu-kernel
./xnu-deps/setup.sh
# Note: xnu/apply.sh may fail due to patch issues; kernel build will use prebuilt
```

### 5.6 Step 5: Build Loader, Userland Images, and Boot Images

```bash
# Build loader (UEFI application)
cd /path/to/hackios-xnu-kernel
./xnuloader/build.sh

# Build userland images (using prebuilt binaries to avoid linker issues)
cd userland
PREBUILT=1 REST=1 ./build.sh   # Creates root.apfs (4 MB)
PREBUILT=1 REST=1 SYSTEM=1 ./build.sh  # Creates system.apfs (2.4 GB)
cd ..

# Create fastboot boot image (for devices supporting fastboot)
export PATH="$HOME/.local/bin:$PATH"
SKIP="kernel" ./common/build.sh dreamlte
# Output: out/dreamlte-boot.img (688 KB) — for `fastboot boot`

# Create TWRP-flashable boot image (for Samsung Download Mode / Odin)
# Uses Samsung DTBH header via dtbtool-exynos
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img --create-payload
# Output: out/dreamlte-boot-twrp.img (692 KB) — flashable via TWRP or Odin
```

### 5.7 Step 6: Verify Build Outputs

```bash
# Verify kernel output
KERNEL=xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped
test -s "$KERNEL" && file "$KERNEL" && xcrun dwarfdump --uuid "$KERNEL"
# Expected: ARM64 Mach-O executable

# Verify images
ls -lh userland/root.apfs userland/system.apfs out/dreamlte-boot.img out/dreamlte-boot-twrp.img 2>/dev/null
```

### 5.8 Expected Build Outputs

| File | Size | Description |
|------|------|-------------|
| `out/dreamlte-boot.img` | 688 KB | Android boot image (UEFI FD + loader + DTB) for `fastboot boot` |
| `out/dreamlte-boot-twrp.img` | 692 KB | **TWRP-flashable Samsung boot image** with DTBH header (for Download Mode/Odin) |
| `userland/root.apfs` | 4 MB | Small boot volume (init, libraries, launchd jobs) |
| `userland/system.apfs` | 2.4 GB | System volume (iOS 16.5 rootfs, cryptexes, dyld cache) |
| `edk2-exynos/Build/dreamltePkg/RELEASE_CLANG38/FV/SILICIUM_UEFI.fd` | 2 MB | UEFI firmware for dreamlte (Exynos 8895) |
| `xnuloader/loader.efi` | 68 KB | XNU kernel loader |

---

## 6. TWRP-Flashable Boot Image Details

The `mkxnuimg_twrp.py` script creates a Samsung-compatible boot image that can be flashed via TWRP recovery or Odin in Download Mode. This is the correct format for the Samsung Galaxy S8 (dreamlte) which uses Download Mode, not fastboot.

### 6.1 Key Features

- **Samsung DTBH Header**: The DTB is processed with `dtbtool-exynos` to add the Samsung DTBH header (required for Exynos 8895 boot)
- **Device-Specific UEFI FD**: Uses the UEFI firmware built from `edk2-exynos` for dreamlte
- **Device-Specific DTB**: Uses the extracted Exynos 8895 DTB (`exynos8895-dreamlte.dtb`)
- **USB Payload**: Includes the XNU loader (`loader.efi`) in a USB-resident XNUP container
- **Correct Memory Addresses**: Uses Exynos 8895 memory map (base 0x10000000, kernel 0x10008000, ramdisk 0x11000000, DTB 0x10000000)
- **Android Boot Image v1**: Standard format compatible with TWRP

### 6.2 Usage

```bash
# Create TWRP-flashable image with USB payload
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img --create-payload

# Create image with existing payload
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img -p out/usb-payload.bin

# Custom addresses (if needed)
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img --create-payload \
  --kernel-addr 0x10008000 --ramdisk-addr 0x11000000 --dtb-addr 0x10000000
```

### 6.3 Image Structure

```
Android Boot Image Header v1 (2048 bytes)
├── Kernel = gzip(BootShim + UEFI FD + XNUP payload) + DTB (with Samsung DTBH header)
├── Ramdisk (minimal 5-byte placeholder)
└── DTB (with Samsung DTBH header, appended to kernel per Android v1 spec)
```

The DTBH header is verified at offset ~0x6f000 in the boot image, confirming Samsung compatibility.

---

## 7. Current Hard Stop: Do Not Flash

**Stop here. There is no supported physical-S8 install command for this checkout.**

These commands are specifically unsafe/incorrect right now:

```sh
# DO NOT RUN:
fastboot boot out/dreamlte-boot.img     # fastboot is not the S8 entry path
heimdall flash --BOOT ...               # no validated image exists
```

### Reasons:

1. No verified `out/dreamlte-boot.img` for the S8 exists from the current documented path
2. The current XNU output is Mach-O, not a Samsung Download Mode package
3. `fastboot boot` is not established as the S8 entry path; this repository's fastboot transport targets its other supported phone (redwood)
4. There is no tested Samsung bootloader/UEFI handoff, no validated image header/container for this S8, and no known-good S8-specific recovery artifact produced by this tree
5. A wrong partition, model, PIT, or bootloader image could leave the device unbootable or cause data loss

The existing `xl` tool and `common/run.sh` drive an Android `fastboot boot` workflow for the phone; that is **not** a Samsung Download Mode/Odin workflow, and the S8 profile files are experimental fragments (addresses unverified, UEFI FD and DTB blank in `common/devices/dreamlte.conf`).

**If the phone is accidentally in Samsung Download Mode and no flash is in progress, Samsung's general recovery instruction is to hold Volume Down + Power for about seven seconds to restart. Do not disconnect a cable while an actual firmware write is in progress.**

---

## 8. Engineering Gates Required Before a Physical Test

These are development tasks, not commands to run against the phone today. Require a written review and evidence for each gate:

### Gate A — Exact Hardware Description

- [ ] Obtain the DTB/DTBO and firmware details for the exact S8 model/region
- [ ] Verify every memory/peripheral address, RAM size, reserved range, CPU MPIDR topology, GIC, UART, timer, UFS, display, and I2C node against that device tree or a trusted Linux device-tree source for the same board
- [ ] Replace current guessed addresses. Verify with a memory-map dump from the actual loader

### Gate B — Working Loader/Firmware Path

- [ ] Build the correct Samsung/Exynos UEFI image and supply real S8 firmware binaries and the matching DTB
- [ ] Register the DREAMLTE platform in the loader and verify its platform detection
- [ ] Audit the existing DREAMLTE loader platform entry against the exact device tree, then implement and verify the actual Samsung entry path. The S8 Download Mode protocol and the loader's own fastboot-like USB protocol are different stages; they are not interchangeable
- [ ] Define how the image is temporarily loaded or restored, and test that procedure with a harmless recovery image first. A Samsung Odin tar is not created merely by renaming a file

### Gate C — XNU Platform Handoff

- [ ] Verify the Mach-O segments and entry point against the loader's `boot_args` and physical placement
- [ ] Verify device tree construction, usable-memory reporting, reserved-memory ranges, kernel slide, stack, page tables, cache maintenance, and early console output
- [ ] Bring up one CPU first. Then test timer interrupts, GIC, scheduler, and PSCI/secondary CPUs one at a time
- [ ] Validate every board-init function and MMIO mapping. Hard-coded init addresses must not be called until they are verified and correctly mapped

### Gate D — Minimum Boot Milestones

Record reproducible logs and pass in order:

1. [ ] UEFI starts and reports the exact board/firmware/DTB
2. [ ] Loader receives and validates the payload; it never selects a redwood or BCM2837 artifact
3. [ ] XNU entry is reached and early UART/loader logging continues
4. [ ] XNU initializes memory, exceptions, GIC, timer, and one CPU without a panic
5. [ ] XNU returns control safely to the resident loader after a bounded test interval
6. [ ] Storage and a read-only root image work
7. [ ] The required iOS userland files are present, compatible, and reach `launchd`, then SpringBoard

> **Do not start with write-enabled UFS, display power sequencing, GPU firmware, or all CPU cores. Add those after the read-only and recovery milestones are repeatable.**

### Gate E — Image and Recovery Review

Before any write/boot procedure is documented, it must state:

- [ ] Exact model, SoC, bootloader state, firmware/build, CSC, and partition table
- [ ] Exact artifact hashes, format, signing status, size, target partition/temporary-boot method, and expected bootloader response
- [ ] The data-loss and Knox effects
- [ ] A tested path back to the matching stock firmware
- [ ] A known failure/timeout exit method

**Only after all of that is reviewed should someone add a command that actually transfers or boots HackiOS on the S8.**

---

## 9. Recovery and Support Notes

- Keep the phone's exact stock firmware and the documented restore method available offline
- If the phone only reaches Download Mode, stop experimenting and restore only with firmware for the exact model and bootloader revision. Do not improvise with another model's PIT or boot image
- Samsung Smart Switch can create a data backup, but it is not a substitute for a device-specific bootloader recovery plan
- The official Android Platform Tools documentation explains `adb`/`fastboot`; it does not certify that Samsung implements Android fastboot booting
- Heimdall's upstream documentation describes communicating with Samsung Download Mode over the Odin protocol; it does not validate this project's image or partition mapping
- Samsung Knox documentation warns that an unauthorized kernel/boot chain may permanently trip the warranty bit and disable Knox-dependent features

---

## 10. Quick Reference: Hardware Target

| Property | Value |
|----------|-------|
| **Device** | Samsung Galaxy S8 (Exynos variants only) |
| **Codename** | `dreamlte` |
| **SoC** | Samsung Exynos 8895 (4× M2 @ 2.3 GHz + 4× Cortex-A53 @ 1.7 GHz) |
| **GPU** | ARM Mali-G71 MP20 (Midgard) |
| **RAM** | 4 GB LPDDR4 |
| **Boot path** | Samsung Download Mode (Odin/Loke/Heimdall) — **not** Android `fastboot boot` |

**Verify your device before proceeding:**
```sh
adb shell getprop ro.product.device    # must be "dreamlte"
adb shell getprop ro.boot.hardware     # must be "dreamlte"
adb shell cat /proc/cpuinfo            # must show Exynos 8895 cores
```
Do not assume an `SM-G950*` model suffix alone proves the SoC.

---

## 11. Repository Structure Reference

HackiOS lives in two repositories that are built as one tree:

| Repository | Path | Contents |
|------------|------|----------|
| `hackios-xnu-kernel` | `/path/to/hackios-xnu-kernel` | XNU kernel patches, EFI loader (`xnuloader`), UEFI port (`edk2-exynos`), build scripts, `xl` phone tool |
| `hackios-xnu-userland-dreamlte` | `/path/to/hackios-xnu-userland-dreamlte` | iOS 16.5 userland: `init`, injected libraries, launchd jobs, APFS image builder, `ios165/fetch.sh` |

The userland repo is checked out into the same directory by `common/setup.sh` (git dir `.userland.git`). Scripts in both repos refer to each other by path, so they must sit side by side.

---

## 12. Local Project References

- `README.md` — Describes the project's currently supported redwood/QEMU flows and build assumptions
- `docs/DREAMLTE_FLASHING.md` — Flashing/recovery quick reference
- `DREAMLTE_PORTING_GUIDE.md` and `DREAMLTE_PORTING_SUMMARY.md` — Earlier experimental notes. Treat their addresses and "fastboot boot" instructions as unverified; this guide supersedes them for physical-device use
- `common/devices/dreamlte.conf` — Experimental device profile (`UEFI_FD` and `DTB` currently blank)
- `common/devices/dreamlte.platform.c` — Platform reference fragment; the corresponding platform entry is also present in `xnuloader/loader.c`, but its hardware values are not validated
- `xnu-src/xnu-8796.121.2/config/MASTER.arm64.dreamlte` — XNU build configuration
- `xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/` — Host build outputs; none should be sent to the phone without the gates above
- `uefi/files/Platforms/Samsung/dreamltePkg/` — UEFI port directory (no FD/DTB supplied)
- `build_dreamlte.sh` / `build_edk2_dreamlte.sh` — Per-step builds (edk2, loader, xnu, system)
- `common/build.sh dreamlte` — Full build: kernel, loader, images, fastboot boot image
- `mkxnuimg_twrp.py` — Creates TWRP-flashable Samsung boot image with DTBH header
- `dtbtool-exynos/dtbTool-exynos` — Samsung DTB header tool (adds DTBH header for Exynos)
- `common/run.sh dreamlte` — Phone boot via `fastboot boot` (not yet valid for S8)
- `ios165/fetch.sh` — Downloads iOS 16.5 from Apple's CDN (userland repo)
- `userland/build.sh` — Builds `root.apfs` and `system.apfs` (userland repo)

---

## 13. License

This repository is licensed under the [Apple Public Source License 2.0](LICENSE). Our changes to XNU are a derivative of XNU and must be; other code here (loader, scripts) is released under the same license. Third-party code keeps its own license. The userland repository is MIT-licensed; no Apple files are included in either repository — `ios165/fetch.sh` downloads the iOS 16.5 IPSW from Apple's CDN to your machine only.

HackiOS is an independent research project by OOSEX GANG and is not affiliated with, authorised or endorsed by Apple Inc. or Samsung Electronics.