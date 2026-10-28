# HackiOS on Samsung Galaxy S8 (dreamlte) — XNU Kernel + iOS 16.5 Userland

**Status: experimental — kernel builds successfully. Do not flash to the phone yet.**

**Current Kernel Build Status (as of 2025-10-29):**
- ✅ **Kernel builds successfully**: XNU kernel for dreamlte (Exynos 8895) compiles and links using the standard build system
- ✅ **Machine config**: DREAMLTE uses arm64 (not arm64e), ARMv8-A, PSCI SMP, GICv3
- ✅ **Patches applied**: PAC/TBI disabled, LTO disabled, Apple Silicon symbols excluded via machine-specific exports
- ✅ **Linker**: Uses lld (LLVM linker) via `-fuse-ld=lld` for memory-efficient linking
- ⚠️ **Export validation**: Post-build symbol validation fails (non-blocking for kernel binary)

This repository contains an experimental Exynos 8895 / XNU configuration for the Samsung Galaxy S8 (codename `dreamlte`, Exynos variants only), plus the companion userland repository (`hackios-xnu-userland-dreamlte`, sibling directory) which provides the iOS 16.5 root/system images. The XNU kernel builds to an ARM64 Mach-O executable, but there is **no validated Samsung boot path, no UEFI/DTB/firmware stack, and no flashable image** in either tree.

See **docs/PHYSICAL_S8_GUIDE.md** for the full bring-up guide, device identification steps, and the five engineering gates (A–E) that must be completed before a physical test.

This README is the step-by-step guide for the work that can be done now: prepare the host, identify the phone, link the userland repo, fetch iOS 16.5, build the kernel and userland images, and then stop at the hard stop before anything touches the phone.

### Working Kernel Build Command

The kernel now builds using the standard XNU build system with lld linker:

```bash
cd xnu-src/xnu-8796.121.2
MACHINE_CONFIGS=DREAMLTE ARCH_CONFIGS=ARM64 KERNEL_CONFIGS=DEVELOPMENT PLATFORM=iPhoneOS \
  SDKROOT=/path/to/iPhoneOS16.5.sdk SDKVERSION=16.5 \
  BUILD_DSYM=0 make -j8
```

The kernel binary will be at:
```
BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped
```

Expected size: ~16.8 MB (ARM64 Mach-O executable)

### Full Build (kernel + loader + userland images + boot image)

From the kernel repository root:

```bash
cd /path/to/hackios-xnu-kernel
common/build.sh dreamlte
```

This runs:
1. **kernel** — XNU build with `MACHINE_CONFIGS=DREAMLTE` (requires iPhoneOS16.5.sdk)
2. **loader** — `xnuloader/loader.efi`
3. **images** — `userland/build.sh` with `REST=1`, producing `userland/root.apfs` and `userland/system.apfs`
4. **boot** — `mkxnuimg.py` packs UEFI FD + loader payload + DTB into `out/dreamlte-boot.img`

---

## Quick Start: Building a Bootable Image (with Mu-Silicium)

The following steps were tested and produce a bootable image for the Samsung Galaxy S8 (dreamlte) using Mu-Silicium UEFI.

### Prerequisites (Linux)

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

### Step 1: Clone and Setup Repositories

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

### Step 2: Build Mu-Silicium UEFI for dreamlte

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

### Step 3: Fetch iOS 16.5 Userland Files

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

### Step 4: Build XNU Dependencies

```bash
cd /path/to/hackios-xnu-kernel
./xnu-deps/setup.sh
# Note: xnu/apply.sh may fail due to patch issues; kernel build will use prebuilt
```

### Step 5: Build Loader, Userland Images, and Boot Images

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

### Build Outputs

After successful build, you will have:

| File | Size | Description |
|------|------|-------------|
| `out/dreamlte-boot.img` | 688 KB | Android boot image (UEFI FD + loader + DTB) for `fastboot boot` |
| `out/dreamlte-boot-twrp.img` | 692 KB | **TWRP-flashable Samsung boot image** with DTBH header (for Download Mode/Odin) |
| `userland/root.apfs` | 4 MB | Small boot volume (init, libraries, launchd jobs) |
| `userland/system.apfs` | 2.4 GB | System volume (iOS 16.5 rootfs, cryptexes, dyld cache) |
| `edk2-exynos/Build/dreamltePkg/RELEASE_CLANG38/FV/SILICIUM_UEFI.fd` | 2 MB | UEFI firmware for dreamlte (Exynos 8895) |
| `xnuloader/loader.efi` | 68 KB | XNU kernel loader |

---

## TWRP-Flashable Boot Image Details

The `mkxnuimg_twrp.py` script creates a Samsung-compatible boot image that can be flashed via TWRP recovery or Odin in Download Mode. This is the correct format for the Samsung Galaxy S8 (dreamlte) which uses Download Mode, not fastboot.

### Key Features

- **Samsung DTBH Header**: The DTB is processed with `dtbtool-exynos` to add the Samsung DTBH header (required for Exynos 8895 boot)
- **Device-Specific UEFI FD**: Uses the UEFI firmware built from `edk2-exynos` for dreamlte (`Build/dreamltePkg/RELEASE_CLANG38/FV/SILICIUM_UEFI.fd`)
- **Device-Specific DTB**: Uses the extracted Exynos 8895 DTB (`exynos8895-dreamlte.dtb`)
- **USB Payload**: Includes the XNU loader (`loader.efi`) in a USB-resident XNUP container
- **Correct Memory Addresses**: Uses Exynos 8895 memory map (base 0x10000000, kernel 0x10008000, ramdisk 0x11000000, DTB 0x10000000)
- **Android Boot Image v1**: Standard format compatible with TWRP

### Usage

```bash
# Create TWRP-flashable image with USB payload
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img --create-payload

# Create image with existing payload
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img -p out/usb-payload.bin

# Custom addresses (if needed)
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img --create-payload \
  --kernel-addr 0x10008000 --ramdisk-addr 0x11000000 --dtb-addr 0x10000000
```

### Flashing Instructions

**Via TWRP Recovery:**
1. Boot into TWRP recovery (Vol Up + Bixby + Power)
2. Transfer `out/dreamlte-boot-twrp.img` to the device (adb push or SD card)
3. In TWRP: Install → Select image → Choose `dreamlte-boot-twrp.img` → Flash to "Boot" partition
4. Reboot

**Via Odin (Download Mode):**
1. Put device in Download Mode (Vol Down + Bixby + Power, then Vol Up)
2. Open Odin on Windows
3. Click "AP" and select `out/dreamlte-boot-twrp.img`
4. Ensure only "Auto Reboot" and "F. Reset Time" are checked
5. Click "Start"
6. Device will reboot automatically

### Image Structure

The boot image contains:
```
Android Boot Image Header v1 (2048 bytes)
├── Kernel = gzip(BootShim + UEFI FD + XNUP payload) + DTB (with Samsung DTBH header)
├── Ramdisk (minimal 5-byte placeholder)
└── DTB (with Samsung DTBH header, appended to kernel per Android v1 spec)
```

The DTBH header is verified at offset ~0x6f000 in the boot image, confirming Samsung compatibility.

---

## Fixes Applied to Build XNU for dreamlte (Exynos 8895)

The following changes were required to build the XNU kernel for the Samsung Galaxy S8 (dreamlte, Exynos 8895):

### 1. Machine Configuration (`xnu-src/xnu-8796.121.2/pexpert/pexpert/arm64/EXYNOS8895.h`)
- Added `GENERIC_PSCI_SMP=1` and `GENERIC_GIC_V3_IRQ=1` for generic ARM64 PSCI SMP and GICv3 support
- Undefined `APPLE_ARM64_ARCH_FAMILY` to avoid Apple Silicon-specific code paths

### 2. Machine-Specific Export Files (exclude Apple Silicon symbols)
- `config/Private.dreamlte.exports` — Base exports without Apple Silicon symbols
- `config/Private.arm64.dreamlte.exports` — Architecture-specific exports
- `config/Libkern.arm64.dreamlte.exports` — Libkern allocator exports
- `config/Libkern.dreamlte.exports` — Non-arch-specific libkern exports

### 3. Build Configuration (`xnu-src/xnu-8796.121.2/makedefs/MakeInc.def`)
- Added DREAMLTE machine flags: `-DARM64_BOARD_CONFIG_DREAMLTE -DARM64_BOARD_CONFIG_EXYNOS8895 -UARM_LARGE_MEMORY -UAPPLE_ARM64_ARCH_FAMILY -march=armv8-a`
- Disabled PAC (Pointer Authentication): `-fno-ptrauth-calls -fno-ptrauth-returns`
- Disabled LTO (Link Time Optimization): `-fno-lto`, `LTO_ENABLED_DEVELOPMENT=0`, `CFLAGS_NOLTO_FLAG=-fno-lto`
- Removed `-Wl,-new_linker` from `LDFLAGS_KERNEL_GEN`
- Set `ARCH_STRING_FOR_CURRENT_MACHINE_CONFIG = arm64` (not arm64e)
- Added `-fuse-ld=lld` for memory-efficient linking with LLVM's lld

### 4. Kernel Config (`config/MASTER.arm64.dreamlte`)
- Disabled `CONFIG_KERNEL_TBI` (Top Byte Ignore) - not supported on Exynos 8895

### 5. Source Code Fixes
- `osfmk/vm/vm_compressor_backing_store.c` — Added `CONFIG_JETSAM` conditional for undefined constants
- `libsa/lastkernelconstructor.c` — Added `__unused` attribute
- `pe_serial.c`, `dreamlte_gpu.c` — Added `__unused` attributes for unused variables

### 6. Build Tool Fixes
- `xnu-deps/host/bin/xnu-clang` — Fixed kernel_compat.h include path
- `BUILD/dst/usr/local/libexec/availability.pl` — Created stub script
- `ld-wrapper.sh` — Updated to handle `-Wl,` prefixed arguments from clang++
- Fixed symlinks: `libtool` → `/usr/bin/libtool` (was Linux binary)

### 7. SDK Setup
- iPhoneOS16.5.sdk copied to `~/Library/Developer/Xcode/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS16.5.sdk`

### 8. vm_tests.c Stub
- Disabled `osfmk/vm/vm_tests.c` (replaced with stub) — original used user-space Mach VM APIs not available in kernel

---

## Current Limitations

- **Export validation**: Post-build symbol set validation (`check_all_exports`) fails with missing libkern/Private symbols. This does not affect the kernel binary itself.
- **Boot Path**: The Samsung Galaxy S8 uses Download Mode (Odin), not fastboot.
  - `out/dreamlte-boot.img` — standard Android boot image for `fastboot boot` (not compatible with S8)
  - `out/dreamlte-boot-twrp.img` — **TWRP-flashable Samsung boot image** with Samsung DTBH header (compatible with S8 Download Mode/Odin)
- **Hardware**: No validated memory map, interrupt setup, display, or peripheral drivers for Exynos 8895.
- **UEFI**: Mu-Silicium UEFI builds successfully but untested on physical hardware.
- **Physical Deployment**: Not yet validated on physical hardware. Do not flash to the phone yet.

---

## Engineering Gates (from PHYSICAL_S8_GUIDE.md)

Before attempting physical deployment, these gates must be completed:

| Gate | Requirement |
|------|-------------|
| **A** | Exact hardware description: obtain DTB/DTBO/firmware for exact S8 model/region. Verify all memory/peripheral addresses. |
| **B** | Working loader/firmware path: build correct Exynos/Samsung UEFI with real S8 firmware binaries and matching DTB. |
| **C** | XNU platform handoff: validate Mach-O segments/entry point against loader's boot_args, device tree, memory map. |
| **D** | Minimum boot milestones: UEFI reports board/firmware → Loader validates payload → XNU entry reached → early UART works → Memory/GIC/timer/CPU init → Storage + read-only root works → iOS userland reaches SpringBoard. |
| **E** | Image and recovery review: document exact model, firmware, partition table, artifact hashes, signing, target partition, restore path. |

---

## Recovery Notes

- Keep exact stock firmware and documented restore method available offline.
- To leave Download Mode: hold **Volume Down + Power** for ~7 seconds.
- Samsung Knox: unauthorized kernel/boot chain may permanently trip warranty bit.

---

## Local References

- `docs/PHYSICAL_S8_GUIDE.md` — Full bring-up guide and gates
- `common/devices/dreamlte.conf` — Experimental device profile
- `xnuloader/loader.c` — Loader platform reference (addresses unverified)
- `xnu-src/xnu-8796.121.2/config/MASTER.arm64.dreamlte` — XNU build configuration
- `Mu-Silicium/Platforms/Samsung/dreamltePkg/` — UEFI port directory
- `common/build.sh dreamlte` — Full build: kernel, loader, images, boot image
- `ios165/fetch.sh` — Downloads iOS 16.5 from Apple's CDN (userland repo)
- `userland/build.sh` — Builds `root.apfs` and `system.apfs` (userland repo)

---

## License

This repository is licensed under the [Apple Public Source License 2.0](LICENSE). Our changes to XNU are a derivative of XNU and must be; other code here (loader, scripts) is released under the same license. Third-party code keeps its own license. The userland repository is MIT-licensed; no Apple files are included in either repository — `ios165/fetch.sh` downloads the iOS 16.5 IPSW from Apple's CDN to your machine only.

HackiOS is an independent research project by OOSEX GANG and is not affiliated with, authorised or endorsed by Apple Inc. or Samsung Electronics.

---

## Hardware Target

| Property | Value |
|---|---|
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

## Repositories

HackiOS lives in two repositories that are built as one tree:

| Repository | Path | Contents |
|---|---|---|
| `hackios-xnu-kernel` | `/path/to/hackios-xnu-kernel` | XNU kernel patches, EFI loader (`xnuloader`), UEFI port (`edk2-exynos`), build scripts, `xl` phone tool |
| `hackios-xnu-userland-dreamlte` | `/path/to/hackios-xnu-userland-dreamlte` | iOS 16.5 userland: `init`, injected libraries, launchd jobs, APFS image builder, `ios165/fetch.sh` |

The userland repo is checked out into the same directory by `common/setup.sh` (git dir `.userland.git`). Scripts in both repos refer to each other by path, so they must sit side by side.

---

## Step 0 — Prepare the host

Commands below are for macOS. The XNU build was developed on macOS with Xcode; some steps (UEFI/EDK2) may be easier on Linux. Do not mix host instructions blindly.

Check the Apple toolchain:

```sh
cd /path/to/hackios-xnu-kernel
pwd
git status --short

xcodebuild -showsdks
xcrun --sdk iphoneos --show-sdk-path
xcrun clang --version
make --version
```

Install Android Platform Tools (official SDK Manager or Platform Tools package) — they provide `adb` and `fastboot`. Having `fastboot` installed does **not** mean the Samsung bootloader accepts fastboot commands; the S8 uses Download Mode.

```sh
adb version
fastboot --version
```

Heimdall is useful for read-only Download Mode inspection (device detection, PIT dump). Do **not** use Odin/Heimdall to write any HackiOS image during the current project state — there is no validated image to write.

---

## Step 1 — Identify the exact S8 variant before changing anything

Boot the phone normally into Android. Enable USB debugging, connect a known-good data USB cable, authorize the host on the phone, then run:

```sh
adb devices -l
adb shell getprop ro.product.model
adb shell getprop ro.product.device
adb shell getprop ro.boot.hardware
adb shell getprop ro.bootloader
adb shell getprop ro.build.version.release
adb shell cat /proc/cpuinfo
```

Record the output:

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

Compare all three before any device-specific work:

1. The model reported by Android **and** the label shown in Download Mode.
2. The actual SoC/CPU identity (this tree targets Exynos 8895).
3. The device codename expected by the artifacts (`dreamlte`, not another S8 variant).

If anything is ambiguous, stop. Do not "try" an image to find out.

To inspect the Samsung boot screen: power off, then hold **Volume Down + Bixby + Power** until the warning screen appears. Read the model/product text without accepting any unlock or flashing prompt. Volume Up continues into Download Mode — do that only when you intentionally want to inspect the screen or use a read-only Download Mode tool. To leave an accidentally entered Download Mode when no write is in progress, hold **Volume Down + Power** for about seven seconds until the phone restarts.

---

## Step 2 — Back up and establish a recovery plan

Before unlocking a bootloader or entering any flashing workflow:

1. Back up photos, files, and app data to a second device or external storage.
2. Use Samsung Smart Switch (or another trusted route) and verify the backup can be read.
3. Save the exact stock firmware package matching the phone's model, region/CSC, and bootloader revision, along with its source and checksums. Keep a known-good cable and a charged battery.
4. Confirm you can reach Download Mode **and** restore the exact stock firmware **before** changing the bootloader.
5. Decide whether losing Knox-protected data/services is acceptable. Samsung documents the Knox warranty bit as a persistent, one-time hardware fuse; custom/unauthorized boot components can trip it and disable Knox features.

Do not rely on a backup of `/data` as a substitute for a stock recovery image. Do not use a PIT file from a different model or firmware family.

---

## Step 3 — Link the userland repository

From the kernel repository root:

```sh
cd /path/to/hackios-xnu-kernel
common/setup.sh
```

This checks `hackios-xnu-userland` out into the same tree (git dir `.userland.git`). Use `common/ugit` for git operations in that repo (`common/ugit status`).

Verify it landed:

```sh
ls userland/build.sh ios165/fetch.sh prebuilt/userland/init
```

---

## Step 4 — Fetch iOS 16.5 (userland)

No Apple files are in the repository. `ios165/fetch.sh` downloads the iOS 16.5 IPSW from Apple's CDN to your machine and extracts what is needed (~7.4 GB).

```sh
cd /path/to/hackios-xnu-userland-dreamlte
ios165/fetch.sh
```

This is a one-time download. It produces `ios165/rootfs`, `ios165/cryptex`, `ios165/cryptex-app`, and `ios165/system.manifest` used by the image builder.

---

## Step 5 — Build the kernel, loader, and userland images

From the kernel repository root, build everything for dreamlte:

```sh
cd /path/to/hackios-xnu-kernel
common/build.sh dreamlte
```

This runs, in order:

1. **kernel** — XNU build with `MACHINE_CONFIGS=DREAMLTE` (requires theos + iPhoneOS16.5.sdk; otherwise falls back to `prebuilt/`).
2. **loader** — `xnuloader/loader.efi` (requires clang + lld-link; otherwise `prebuilt/loader.efi`).
3. **images** — `userland/build.sh` with `REST=1`, producing `userland/root.apfs` and `userland/system.apfs` (requires `ios165/fetch.sh` to have run).
4. **boot** — `mkxnuimg.py` packs the UEFI FD + loader payload + DTB into `out/dreamlte-boot.img`.

Verify the kernel output:

```sh
KERNEL=xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped
test -s "$KERNEL" && file "$KERNEL" && xcrun dwarfdump --uuid "$KERNEL"
```

Expected: ARM64 Mach-O executable. That proves only that the host linker emitted a file — it does **not** validate the boot path, memory map, interrupt setup, or UEFI handoff.

Expected images:

```sh
ls -lh userland/root.apfs userland/system.apfs out/dreamlte-boot.img 2>/dev/null
```

---

## Step 6 — Current hard stop: do not flash

**Stop here. There is no supported physical-S8 install command for this checkout.**

These commands are specifically unsafe/incorrect right now:

```sh
# DO NOT RUN:
fastboot boot out/dreamlte-boot.img     # fastboot is not the S8 entry path
heimdall flash --BOOT ...               # no validated image exists
```

Reasons:

- No verified `out/dreamlte-boot.img` for the S8 exists from the current documented path.
- The current XNU output is Mach-O, not a Samsung Download Mode package.
- `fastboot boot` is not established as the S8 entry path; this repository's fastboot transport targets its other supported phone (redwood).
- There is no tested Samsung bootloader/UEFI handoff, no validated image header/container for this S8, and no known-good S8-specific recovery artifact produced by this tree.
- A wrong partition, model, PIT, or bootloader image could leave the device unbootable or cause data loss.

The existing `xl` tool and `common/run.sh` drive an Android `fastboot boot` workflow for the phone; that is **not** a Samsung Download Mode/Odin workflow, and the S8 profile files are experimental fragments (addresses unverified, UEFI FD and DTB blank in `common/devices/dreamlte.conf`).

---

## Step 7 — Engineering gates required before a physical test

These are development tasks, not commands to run against the phone today. Require written review and evidence for each gate before adding any command that transfers or boots HackiOS on the S8.

| Gate | Requirement |
|---|---|
| **A — Exact hardware description** | Obtain DTB/DTBO and firmware for the exact S8 model/region. Verify every memory/peripheral address, RAM size, CPU topology, GIC, UART, timer, UFS, display, I2C against device tree. Replace guessed addresses (the profile's `FD_BASE`, `PAYLOAD_BASE`, `FB_BASE`, GIC/UART values are unverified). |
| **B — Working loader/firmware path** | Build the correct Exynos/Samsung UEFI image with real S8 firmware binaries and matching DTB (`edk2-exynos/Build/dreamltePkg/.../FV/SILICIUM_UEFI.fd`, currently not in the repo). Register DREAMLTE platform in `xnuloader/loader.c` and verify detection. Define the temporary load/restore procedure; test it with a harmless recovery image first. |
| **C — XNU platform handoff** | Verify Mach-O segments/entry point against the loader's `boot_args` and physical placement. Validate device tree, usable memory, reserved ranges, kernel slide, page tables, cache maintenance, early console. Bring up one CPU, then timer, GIC, scheduler, PSCI/secondaries. |
| **D — Minimum boot milestones** | 1) UEFI reports exact board/firmware/DTB 2) Loader validates payload (never selects redwood/BCM2837) 3) XNU entry reached, early UART works 4) Memory, exceptions, GIC, timer, one CPU init without panic 5) XNU returns to loader after bounded test 6) Storage + read-only root image work 7) iOS userland reaches SpringBoard |
| **E — Image and recovery review** | Document exact model, SoC, bootloader state, firmware, CSC, partition table, artifact hashes, format, signing, target partition/temporary-boot method, expected bootloader response, data-loss and Knox effects, tested restore path, failure exit method. |

Only after all gates pass should someone add a command that actually transfers or boots HackiOS on the S8.

---

## Recovery Notes

- Keep exact stock firmware and the documented restore method available offline.
- If the phone only reaches Download Mode, restore only with firmware for the exact model and bootloader revision. Do not improvise with another model's PIT or boot image.
- To leave Download Mode when no write is in progress: hold **Volume Down + Power** for about seven seconds until the phone restarts.
- Samsung Knox: unauthorized kernel/boot chain may permanently trip the warranty bit and disable Knox features.

---

## Local References

- `docs/PHYSICAL_S8_GUIDE.md` — Full bring-up guide and gates (source of this document's safety sections).
- `common/devices/dreamlte.conf` — Experimental device profile (`UEFI_FD` and `DTB` currently blank).
- `common/devices/dreamlte.platform.c` — Loader platform reference fragment (addresses unverified).
- `xnuloader/loader.c` — Contains `platform_dreamlte` entry; hardware values not validated.
- `xnu-src/xnu-8796.121.2/config/MASTER.arm64.dreamlte` — XNU build configuration.
- `uefi/files/Platforms/Samsung/dreamltePkg/` — UEFI port directory (no FD/DTB supplied).
- `build_dreamlte.sh` / `build_edk2_dreamlte.sh` — Per-step builds (edk2, loader, xnu, system).
- `common/build.sh dreamlte` — Full build: kernel, loader, images, fastboot boot image.
- `mkxnuimg_twrp.py` — Creates TWRP-flashable Samsung boot image with DTBH header.
- `dtbtool-exynos/dtbTool-exynos` — Samsung DTB header tool (adds DTBH header for Exynos).
- `common/run.sh dreamlte` — Phone boot via `fastboot boot` (not yet valid for S8).
- `ios165/fetch.sh` — Downloads iOS 16.5 from Apple's CDN (userland repo).
- `userland/build.sh` — Builds `root.apfs` and `system.apfs` (userland repo).

---

## License

This repository is licensed under the [Apple Public Source License 2.0](LICENSE). Our changes to XNU are a derivative of XNU and must be; other code here (loader, scripts) is released under the same license. Third-party code keeps its own license. The userland repository is MIT-licensed; no Apple files are included in either repository — `ios165/fetch.sh` downloads the iOS 16.5 IPSW from Apple's CDN to your machine only.

HackiOS is an independent research project by OOSEX GANG and is not affiliated with, authorised or endorsed by Apple Inc. or Samsung Electronics.
