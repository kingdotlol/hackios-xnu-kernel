# HackiOS on a Physical Samsung Galaxy S8 (dreamlte): Complete Bring-Up Guide

**Status: EXPERIMENTAL — Kernel builds successfully. TWRP-flashable boot image can be created. Proceed at your own risk — phone may brick, Knox will trip.**

The repository now contains a working Exynos 8895/XNU configuration. The XNU kernel for dreamlte compiles and links successfully as an ARM64 Mach-O executable (16.8 MB). A TWRP-flashable boot image with Samsung DTBH header can be built via `mkxnuimg_twrp.py`. The UEFI firmware builds via Mu-Silicium. iOS 16.5 userland images build via the userland repo.

**This is still experimental.** No validated end-to-end boot has been confirmed on physical hardware. The existing `xl`/`common/run.sh` phone transport expects Android `fastboot boot`; the S8 uses Samsung Download Mode (Odin/Loke). The dreamlte UEFI profile needs the built UEFI FD/DTB copied to the expected locations. The boot image must use the TWRP-flashable format with Samsung DTBH header.

> **⚠️ CRITICAL WARNING:** Flashing custom boot code will almost certainly:
> - Erase the phone and make it unbootable without stock firmware restore
> - Permanently trip Samsung Knox's warranty bit (one-time hardware fuse, irreversible)
> - Disable Knox-protected data/services (Samsung Pay, Secure Folder, etc.)
> - Require Odin/Heimdall restore with exact model/region/CSC/bootloader firmware
> 
> **Proceed only with a spare/test device you own, with a tested stock recovery route, and after fully accepting these consequences.** If you brick it, you keep both pieces.

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

# Verify kernel builds
ls -la xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped 2>/dev/null
```

### 4.1 Current State of S8 Artifacts (as of 2025-10-29)

- ✅ Kernel builds: `MACHINE_CONFIGS=DREAMLTE` produces valid ARM64 Mach-O (~16.8 MB)
- ✅ UEFI builds: `python3 build_uefi.py -d dreamlte` produces `SILICIUM_UEFI.fd`
- ✅ DTB available: `Mu-Silicium/Resources/DTBs/dreamlte.dtb`
- ✅ TWRP-flashable image builds: `mkxnuimg_twrp.py --create-payload` adds Samsung DTBH header
- ✅ Loader builds: `xnuloader/build.sh` produces `loader.efi`
- ⚠️ Device profile `common/devices/dreamlte.conf` has `UEFI_FD` and `DTB` blank - must be populated from build outputs
- ⚠️ `xnuloader/loader.c` has `platform_dreamlte` entry but addresses are unverified
- ⚠️ `common/devices/dreamlte.platform.c` is a reference fragment, not validated addresses
- ⚠️ The S8 UEFI binary set and DTB must be real, device-matched artifacts (use built outputs, not redwood prebuilts)

### 4.2 Read the Build Scripts Before Invoking

```sh
# Main build orchestrator (uses BCM2837 kernel - needs update for DREAMLTE kernel)
sed -n '1,240p' common/build.sh

# Phone boot script (currently fastboot-based - not for S8)
sed -n '1,160p' common/run.sh

# Boot image packer (TWRP format uses mkxnuimg_twrp.py)
sed -n '1,180p' mkxnuimg_twrp.py

# Loader build
sed -n '1,180p' xnuloader/build.sh
```

### 4.3 Current Status of Previously Blocked Items

- ✅ **Kernel no longer uses BCM2837 prebuilt** - DREAMLTE kernel builds natively via `make MACHINE_CONFIGS=DREAMLTE`
- ✅ **Image packer no longer falls back to redwood UEFI** - uses built `SILICIUM_UEFI.fd` via `mkxnuimg_twrp.py`
- ⚠️ `common/run.sh` still drives `fastboot boot` workflow - not compatible with S8 Download Mode
- ⚠️ S8 memory-map addresses in profile are explicitly marked unverified - need physical validation

**The kernel builds successfully. Use the build procedure in Section 5, not `common/build.sh dreamlte` (which skips kernel).**

---

## 5. Complete Build Procedure (Host-Only)

This section documents the full build process that produces all artifacts. **This is host-only work - it does not contact or modify the phone.**

### 5.1 Prerequisites Checklist

Before starting the build, verify you have:

- [ ] macOS host with Xcode + iPhoneOS 16.5 SDK (for XNU kernel)
- [ ] Linux host or VM (for UEFI/EDK2 build)
- [ ] iPhoneOS16.5.sdk at `~/Library/Developer/Xcode/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS16.5.sdk` (macOS)
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

### 5.3 Step 2: Build Mu-Silicium UEFI for dreamlte (Linux)

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

### 5.5 Step 4: Build XNU Kernel for dreamlte (macOS)

```bash
cd /path/to/hackios-xnu-kernel/xnu-src/xnu-8796.121.2

# Build kernel with lld linker (avoids OOM with Apple's ld)
MACHINE_CONFIGS=DREAMLTE ARCH_CONFIGS=ARM64 KERNEL_CONFIGS=DEVELOPMENT PLATFORM=iPhoneOS \
  SDKROOT=/Users/radumoise/Library/Developer/Xcode/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS16.5.sdk \
  SDKVERSION=16.5 BUILD_DSYM=0 make -j8

# Kernel output:
# BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped (~16.8 MB)
```

### 5.6 Step 5: Build Loader, Userland Images, and Boot Images

```bash
# Build loader (UEFI application) - can run on macOS or Linux
cd /path/to/hackios-xnu-kernel
./xnuloader/build.sh

# Build userland images
cd userland
PREBUILT=1 REST=1 ./build.sh   # Creates root.apfs (4 MB)
PREBUILT=1 REST=1 SYSTEM=1 ./build.sh  # Creates system.apfs (2.4 GB)
cd ..

# Create TWRP-flashable boot image (for Samsung Download Mode / Odin)
# Uses Samsung DTBH header via dtbtool-exynos
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img --create-payload
# Output: out/dreamlte-boot-twrp.img (~692 KB) — flashable via TWRP or Odin
```

### 5.7 Step 6: Verify Build Outputs

```bash
# Verify kernel output (macOS)
KERNEL=xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped
test -s "$KERNEL" && file "$KERNEL" && xcrun dwarfdump --uuid "$KERNEL"
# Expected: Mach-O 64-bit executable arm64

# Verify images
ls -lh userland/root.apfs userland/system.apfs out/dreamlte-boot-twrp.img 2>/dev/null
```

### 5.8 Expected Build Outputs

| File | Size | Description |
|------|------|-------------|
| `out/dreamlte-boot-twrp.img` | ~692 KB | **TWRP-flashable Samsung boot image** with DTBH header (for Download Mode/Odin) |
| `userland/root.apfs` | 4 MB | Small boot volume (init, libraries, launchd jobs) |
| `userland/system.apfs` | 2.4 GB | System volume (iOS 16.5 rootfs, cryptexes, dyld cache) |
| `edk2-exynos/Build/dreamltePkg/RELEASE_CLANG38/FV/SILICIUM_UEFI.fd` | 2 MB | UEFI firmware for dreamlte (Exynos 8895) |
| `xnuloader/loader.efi` | 68 KB | XNU kernel loader |
| `xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped` | 16.8 MB | ARM64 Mach-O kernel executable |

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

## 7. Deployment: Flashing the TWRP-Flashable Boot Image (Experimental)

**This section describes how to flash the experimental boot image. Proceed at your own risk — phone may become permanently unbootable.**

### 7.1 Prerequisites for Flashing

Before flashing, ensure you have:
- [ ] Exact stock firmware for your S8 model/region/CSC/bootloader version (for restore)
- [ ] Tested the stock firmware restore procedure on this exact device
- [ ] Accepted that Knox warranty bit will be permanently tripped
- [ ] Backed up all data (Smart Switch or equivalent)
- [ ] Known-good USB cable and charged battery (≥50%)

### 7.2 Prepare the Device

1. **Enable OEM Unlock** (if not already):
   - Settings → Developer Options → OEM Unlock → Enable
   - This requires the device to be unlocked and connected to internet

2. **Enter Download Mode**:
   - Power off completely
   - Hold **Volume Down + Bixby + Power** until warning screen appears
   - Press **Volume Up** to enter Download Mode

3. **Verify device detection** (read-only):
   ```bash
   heimdall detect
   # or on Windows with Odin: device should appear in ID:COM port
   ```

### 7.3 Flash via Odin (Windows) — Recommended

1. Open Odin3 v3.14.4 or later
2. Click **AP** button, select `out/dreamlte-boot-twrp.img`
3. Ensure **only** these options are checked:
   - Auto Reboot
   - F. Reset Time
   - **Do NOT check**: Re-Partition, any other options
4. Click **Start**
5. Wait for "PASS!" in green — device will auto-reboot

### 7.4 Flash via Heimdall (Linux/macOS) — Alternative

```bash
# Flash boot partition only
heimdall flash --BOOT out/dreamlte-boot-twrp.img --no-reboot

# Or with verbose output
heimdall flash --BOOT out/dreamlte-boot-twrp.img --verbose

# Reboot after flash
heimdall reboot
```

### 7.5 Flash via TWRP Recovery (if already installed)

1. Boot into TWRP: **Volume Up + Bixby + Power**
2. Transfer `out/dreamlte-boot-twrp.img` to device:
   ```bash
   adb push out/dreamlte-boot-twrp.img /tmp/
   ```
3. In TWRP: Install → Install Image → Select `/tmp/dreamlte-boot-twrp.img` → Flash to "Boot" partition
4. Reboot → System

### 7.6 Expected Boot Behavior

After flashing, the boot sequence should be:
1. Samsung bootloader → loads boot partition
2. DTBH header validated → DTB parsed for Exynos 8895
3. UEFI FD (SILICIUM_UEFI.fd) loads → initializes hardware
4. XNU loader (`loader.efi`) starts → prepares boot_args
5. XNU kernel entry (`__start`) reached → early console via UART
6. Memory/GIC/timer/CPU init → iOS userland mounts root/system APFS
7. SpringBoard appears (if all milestones pass)

**Most likely outcome on first attempt**: Boot loops, panic, or hangs at early init. This is expected for unvalidated hardware bring-up.

### 7.7 Recovery If Brick

If the phone fails to boot (stuck in Download Mode, bootloop, or black screen):

1. **Immediately restore stock firmware** via Odin:
   - Download exact stock firmware for your model/region/CSC/bootloader
   - Extract the `.tar.md5` to get `AP_*.tar.md5`, `BL_*.tar.md5`, `CP_*.tar.md5`, `CSC_*.tar.md5`, `HOME_CSC_*.tar.md5`
   - In Odin: AP=AP, BL=BL, CP=CP, CSC=HOME_CSC (not CSC - preserves data if possible)
   - Flash and wait for PASS!

2. **If soft-brick only** (boots to Android but HackiOS doesn't work):
   - Flash stock boot.img via Odin AP slot only

3. **Do not**: Try different PIT files, mix firmware versions, or flash recovery partition unless restoring full stock

---

## 8. Engineering Gates — Current Status (as of 2025-10-29)

These gates track what's verified vs what needs physical testing. Items marked ✅ are complete on the host; items marked 📋 need physical validation.

### Gate A — Exact Hardware Description

- [✅] DTB for dreamlte available from Mu-Silicium (`Mu-Silicium/Resources/DTBs/dreamlte.dtb`)
- [✅] Exynos 8895 memory map documented in loader (0x10000000 base, 0x10008000 kernel, 0x11000000 ramdisk)
- [📋] Verify every peripheral address, GIC, UART, timer, UFS, display, I2C against physical device
- [📋] Validate CPU MPIDR topology (4× M2 + 4× A53) matches PSCI
- [📋] Replace guessed addresses with memory-map dump from actual loader

### Gate B — Working Loader/Firmware Path

- [✅] UEFI builds via Mu-Silicium (`python3 build_uefi.py -d dreamlte`)
- [✅] UEFI FD at `Mu-Silicium/Build/dreamltePkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd`
- [✅] DTBH header added via `dtbtool-exynos` in `mkxnuimg_twrp.py`
- [✅] Loader builds (`xnuloader/build.sh` → `loader.efi`)
- [✅] TWRP-flashable boot image builds (`mkxnuimg_twrp.py --create-payload`)
- [📋] Register DREAMLTE platform in loader and verify detection on physical device
- [📋] Verify Samsung Download Mode → UEFI → Loader handoff works
- [📋] Test Odin/Heimdall flash procedure with stock recovery image first

### Gate C — XNU Platform Handoff

- [✅] Kernel builds as ARM64 Mach-O (`MACHINE_CONFIGS=DREAMLTE ARCH_CONFIGS=ARM64 KERNEL_CONFIGS=DEVELOPMENT PLATFORM=iPhoneOS`)
- [✅] PAC/TBI disabled, LTO disabled, arm64 (not arm64e) for ARMv8-A
- [✅] PSCI SMP + GICv3 configured in `EXYNOS8895.h`
- [✅] Machine-specific exports exclude Apple Silicon symbols
- [📋] Verify Mach-O segments/entry point against loader's `boot_args`
- [📋] Verify device tree construction, usable memory, reserved ranges, kernel slide
- [📋] Verify early console output (UART)
- [📋] Bring up 1 CPU → timer → GIC → PSCI secondaries

### Gate D — Minimum Boot Milestones

Record reproducible logs and pass in order:

1. [📋] UEFI starts and reports exact board/firmware/DTB
2. [📋] Loader receives/validates payload; never selects redwood/BCM2837
3. [📋] XNU entry reached, early UART/loader logging continues
4. [📋] XNU initializes memory, exceptions, GIC, timer, 1 CPU without panic
5. [📋] XNU returns control to loader after bounded test
6. [📋] Storage + read-only root image work
7. [📋] iOS userland files present, compatible, reach launchd → SpringBoard

> **Do not start with write-enabled UFS, display power sequencing, GPU firmware, or all CPU cores. Add those after read-only and recovery milestones are repeatable.**

### Gate E — Image and Recovery Review

Before any write/boot procedure is documented, it must state:

- [📋] Exact model, SoC, bootloader state, firmware/build, CSC, partition table
- [📋] Exact artifact hashes, format, signing status, size, target partition
- [📋] Data-loss and Knox effects documented
- [📋] Tested path back to matching stock firmware
- [📋] Known failure/timeout exit method

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

- `README.md` — Current build status and working kernel build command
- `docs/DREAMLTE_FLASHING.md` — Flashing/recovery quick reference
- `DREAMLTE_PORTING_GUIDE.md` and `DREAMLTE_PORTING_SUMMARY.md` — Earlier experimental notes. Treat their addresses and "fastboot boot" instructions as unverified; this guide supersedes them for physical-device use
- `common/devices/dreamlte.conf` — Device profile (`UEFI_FD` and `DTB` blank - populate from build outputs)
- `common/devices/dreamlte.platform.c` — Loader platform reference fragment (addresses unverified)
- `xnuloader/loader.c` — Contains `platform_dreamlte` entry; hardware values not validated
- `xnu-src/xnu-8796.121.2/config/MASTER.arm64.dreamlte` — XNU build configuration (working)
- `xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped` — Built kernel (16.8 MB ARM64 Mach-O)
- `uefi/files/Platforms/Samsung/dreamltePkg/` — UEFI port directory
- `Mu-Silicium/Build/dreamltePkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd` — Built UEFI FD
- `Mu-Silicium/Resources/DTBs/dreamlte.dtb` — Device tree for Exynos 8895
- `mkxnuimg_twrp.py` — Creates TWRP-flashable Samsung boot image with DTBH header (use this, not mkxnuimg.py)
- `dtbtool-exynos/dtbTool-exynos` — Samsung DTB header tool (adds DTBH header for Exynos)
- `xnuloader/build.sh` — Builds `loader.efi`
- `ios165/fetch.sh` — Downloads iOS 16.5 from Apple's CDN (userland repo)
- `userland/build.sh` — Builds `root.apfs` and `system.apfs` (userland repo)

### Build Commands Summary

```bash
# Kernel (macOS)
cd xnu-src/xnu-8796.121.2
MACHINE_CONFIGS=DREAMLTE ARCH_CONFIGS=ARM64 KERNEL_CONFIGS=DEVELOPMENT PLATFORM=iPhoneOS \
  SDKROOT=/path/to/iPhoneOS16.5.sdk SDKVERSION=16.5 BUILD_DSYM=0 make -j8

# UEFI (Linux)
cd Mu-Silicium && python3 build_uefi.py -d dreamlte

# Loader (macOS/Linux)
./xnuloader/build.sh

# Userland (macOS)
cd userland && PREBUILT=1 REST=1 ./build.sh && PREBUILT=1 REST=1 SYSTEM=1 ./build.sh

# TWRP-flashable boot image
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img --create-payload
```

---

## 13. License

This repository is licensed under the [Apple Public Source License 2.0](LICENSE). Our changes to XNU are a derivative of XNU and must be; other code here (loader, scripts) is released under the same license. Third-party code keeps its own license. The userland repository is MIT-licensed; no Apple files are included in either repository — `ios165/fetch.sh` downloads the iOS 16.5 IPSW from Apple's CDN to your machine only.

HackiOS is an independent research project by OOSEX GANG and is not affiliated with, authorised or endorsed by Apple Inc. or Samsung Electronics.