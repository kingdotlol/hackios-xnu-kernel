# HackiOS on a physical Samsung Galaxy S8 (dreamlte): hands-on guide

**Status: this is a bring-up guide, not a claim that the current tree can boot the S8. Do not flash HackiOS to the phone yet.**

The repository contains an experimental Exynos 8895/XNU configuration and some early device files, but it does not
yet contain a verified end-to-end Samsung boot path. The XNU build has produced an ARM64 Mach-O kernel executable,
but that executable is not a Samsung/Odin image and is not, by itself, bootable. The existing `xl`/`common/run.sh`
phone transport expects Android `fastboot boot`; the S8 normally uses Samsung Download Mode (Odin/Loke), not that
transport. The dreamlte UEFI profile has no supplied UEFI FD/DTB, the loader platform file is still a fragment, and
the root build script selects the BCM2837 kernel path. Consequently, **there is no safe command today that installs
this build on an S8**.

This guide takes you through the safe, useful work that can be done now, explains the stop point, and defines the
gates that must be passed before a temporary boot or flash procedure is written. It deliberately does not provide
an `heimdall flash`, Odin AP, PIT, or raw-partition command: there is no validated S8 image to flash.

> Flashing custom boot code can erase the phone, make it unbootable, permanently trip Samsung Knox's warranty bit,
> and make Knox-protected data/services unavailable. A restore does not reverse the one-time warranty fuse.
> Proceed only with a phone you own, with a tested stock recovery route, and after accepting those consequences.

## 0. Terminology and supported target

- **dreamlte** means the Exynos Galaxy S8 target in this tree. It is not a generic name for every Galaxy S8.
- Do not assume an `SM-G950*` suffix alone proves the SoC. Read the exact model and board/product values from the
  phone, then independently verify that the device is Exynos 8895 and that the kernel/loader identify it as
  `dreamlte`.
- This project’s `common/run.sh`, `xl`, `mkxnuimg.py`, and prebuilt artifacts are currently wired around the
  **redwood** (POCO) phone flow and QEMU. The S8 profile files are experimental and are not evidence that the S8
  launch path works.
- A kernel file is only one stage. A physical boot also needs a device-compatible boot entry, correct firmware and
  device tree, a loader that builds valid XNU `boot_args`, working memory/interrupt/timer/CPU support, and a usable
  iOS root/system image. Reaching the XNU linker is not reaching the lock screen.

## 1. Prepare the host

The commands below are for the HackiOS workspace on macOS. Use a Linux host if the project’s build dependencies
or device tools require it; do not mix instructions for the two hosts blindly.

```sh
cd /Users/radumoise/Desktop/hackios-xnu-kernel
pwd
git status --short
```

Do not run `git reset`, `git clean`, or overwrite local files to “start fresh.” This workspace contains local porting
changes and build artifacts.

Check the local Apple toolchain and SDK:

```sh
xcodebuild -showsdks
xcrun --sdk iphoneos --show-sdk-path
xcrun clang --version
make --version
```

The local build was linked with Apple Clang 21 and the iPhoneOS 27.1 SDK. XNU here is the `xnu-8796.121.2`
source baseline; a successful compile with a newer SDK is not proof that the kernel is compatible with an iOS 16.5
userspace or with Samsung hardware.

Install Android Platform Tools using the official Android SDK Manager or the official Platform Tools package.
They provide `adb` and `fastboot`; **having `fastboot` installed does not mean the Samsung bootloader accepts
fastboot commands**.

```sh
adb version
fastboot --version
```

Heimdall is an open-source implementation of Samsung’s Download Mode protocol. It is useful for non-writing device
detection/PIT inspection when appropriate, but its presence does not make an unvalidated HackiOS image flashable.
Do not use Odin/Heimdall to write any HackiOS image during the current project state.

## 2. Identify the exact S8 variant before changing anything

Boot the phone normally into Android. Enable USB debugging only if you are comfortable doing so, connect a known
data-capable USB cable, authorize the host on the phone, then run:

```sh
adb devices -l
adb shell getprop ro.product.model
adb shell getprop ro.product.device
adb shell getprop ro.boot.hardware
adb shell getprop ro.bootloader
adb shell getprop ro.build.version.release
adb shell cat /proc/cpuinfo
```

Record the output in a local text file, for example:

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

Compare the results with all three of these before considering any device-specific work:

1. The label/model reported by Android and the label printed in Download Mode.
2. The actual SoC/CPU identity (the target config in this tree is Exynos 8895).
3. The device codename expected by the artifacts you intend to run (`dreamlte`, not another S8 variant).

If the model, SoC, codename, or bootloader status is ambiguous, stop. Do not “try” an image to find out.

If you want to inspect the Samsung boot screen, power the phone off, then hold **Volume Down + Bixby + Power**
until the warning screen appears. Read the model/product text without accepting any unlock or flashing prompt.
Pressing **Volume Up** continues into Download Mode; do that only when you intentionally want to inspect the
screen or use a read-only Download Mode tool. This is Samsung Download Mode, not Android `fastboot`.
To leave an accidentally entered Download Mode when no write is in progress, hold **Volume Down + Power** for
about seven seconds until the phone restarts.

## 3. Back up and establish a recovery plan

Before unlocking a bootloader or entering a flashing workflow:

1. Back up photos, files, and app data to a second device/computer or external storage.
2. Use Samsung Smart Switch or another trusted backup route, then verify that the backup can be read.
3. Save the exact stock firmware package matching the phone’s model and region/CSC, along with its source and
   checksums. Keep a known-good cable and a charged battery.
4. Confirm that you can reach Download Mode and can restore the exact stock firmware **before** changing the
   bootloader.
5. Decide whether losing Knox-protected data/services is acceptable. Knox documents its warranty bit as a
   persistent, one-time hardware fuse; custom/unauthorized boot components can affect Knox features.

Do not rely on a backup of `/data` as a substitute for a stock recovery image. Do not use a PIT file from a
different model or firmware family.

## 4. Read-only project preflight

From the repository root, inspect the experimental S8 files and boot artifacts:

```sh
cd /Users/radumoise/Desktop/hackios-xnu-kernel

cat common/devices/dreamlte.conf
sed -n '1,180p' common/devices/dreamlte.platform.c
find uefi/files/Platforms/Samsung/dreamltePkg -type f -print
find uefi/files/Binaries/dreamlte -type f -print
find prebuilt -maxdepth 3 -type f -print
```

The device profile currently leaves `UEFI_FD` and `DTB` blank. `xnuloader/loader.c` does contain a
`platform_dreamlte` entry and lists it for framebuffer-based detection, but the addresses/comments are still
unverified. `common/devices/dreamlte.platform.c` is a separate reference fragment, not proof that those values
are correct. The S8 UEFI binary set and an S8 DTB must be real, device-matched artifacts, not the repository’s
redwood prebuilt UEFI/DTB.

Read the scripts before invoking them:

```sh
sed -n '1,240p' common/build.sh
sed -n '1,160p' common/run.sh
sed -n '1,180p' mkxnuimg.py
sed -n '1,180p' xnuloader/build.sh
```

Important current blockers visible in those scripts:

- `common/build.sh` selects a `DEVELOPMENT_ARM64_BCM2837` kernel artifact; this is not the DREAMLTE kernel output.
- The image packer falls back to redwood UEFI files when no S8 UEFI build is supplied.
- `common/run.sh` drives a standard `fastboot boot` workflow; it is not a Samsung Download Mode/Odin workflow.
- The expected S8 memory-map addresses in the profile are explicitly marked unverified.

Do not run `common/build.sh dreamlte` or `common/run.sh dreamlte` expecting them to boot the S8. First fix and
review those integration points and test them with a non-destructive artifact checker.

## 5. Build the current DREAMLTE XNU kernel (host-only)

This stage builds on the Mac; it does not contact or modify the phone. From the XNU source directory:

```sh
cd /Users/radumoise/Desktop/hackios-xnu-kernel/xnu-src/xnu-8796.121.2

SDKROOT="$(xcrun --sdk iphoneos --show-sdk-path)"
printf 'SDKROOT=%s\n' "$SDKROOT"

set -o pipefail
make SDKROOT="$SDKROOT" \
  PLATFORM=iPhoneOS \
  ARCH_STRING_FOR_CURRENT_MACHINE_CONFIG=arm64 \
  ARCH_CONFIGS=ARM64 \
  MACHINE_CONFIGS=DREAMLTE \
  KERNEL_CONFIGS=DEVELOPMENT \
  2>&1 | tee /tmp/dreamlte-ios-build.log
```

The direct output to inspect is:

```sh
KERNEL=BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped
test -s "$KERNEL"
file "$KERNEL"
xcrun dwarfdump --uuid "$KERNEL"
```

Expected file type for the current linker output is an ARM64 Mach-O executable. That proves only that the host
linker emitted a file; it does **not** prove that the XNU entry point, image layout, boot arguments, memory map,
interrupt setup, or S8 boot transport are correct.

At the time this guide was written, the DREAMLTE XNU link produced that Mach-O file. The overall `make` command
then failed in the dSYM packaging step because the restricted build environment denied Bash process substitution
through `/dev/fd`. Treat that as a separate build-packaging issue, not as a boot test. Do not mistake the
`.unstripped` file for `out/dreamlte-boot.img`.

## 6. Current hard stop: do not flash

**Stop here. There is no supported physical-S8 install command for this checkout.**

These commands are specifically unsafe/incorrect right now:

```sh
# DO NOT RUN:
fastboot boot out/dreamlte-boot.img
heimdall flash --BOOT ...
```

Reasons:

- No verified `out/dreamlte-boot.img` for the S8 exists from the current documented path.
- The current XNU output is Mach-O, not a Samsung Download Mode package.
- `fastboot boot` is not established as the S8 entry path; this repository’s documented fastboot transport is for
  its other supported phone.
- There is no tested Samsung bootloader/UEFI handoff, no validated image header/container for this S8, and no
  known-good S8-specific recovery artifact produced by this tree.
- A wrong partition, model, PIT, or bootloader image could leave the device unbootable or cause data loss.

If the phone is accidentally in Samsung Download Mode and no flash is in progress, Samsung’s general recovery
instruction is to hold **Volume Down + Power** for about seven seconds to restart. Do not disconnect a cable while
an actual firmware write is in progress.

## 7. Engineering gates required before a physical test

These are development tasks, not commands to run against the phone today. Require a written review and evidence
for each gate:

### Gate A — exact hardware description

- Obtain the DTB/DTBO and firmware details for the exact S8 model/region.
- Verify every memory/peripheral address, RAM size, reserved range, CPU MPIDR topology, GIC, UART, timer, UFS,
  display, and I2C node against that device tree or a trusted Linux device-tree source for the same board.
- Replace current guessed addresses. Verify with a memory-map dump from the actual loader.

### Gate B — working loader/firmware path

- Build the correct Samsung/Exynos UEFI image and supply real S8 firmware binaries and the matching DTB.
- Register the DREAMLTE platform in the loader and verify its platform detection.
- Audit the existing DREAMLTE loader platform entry against the exact device tree, then implement and verify the
  actual Samsung entry path. The S8 Download Mode protocol and the loader’s own
  fastboot-like USB protocol are different stages; they are not interchangeable.
- Define how the image is temporarily loaded or restored, and test that procedure with a harmless recovery image
  first. A Samsung Odin tar is not created merely by renaming a file.

### Gate C — XNU platform handoff

- Verify the Mach-O segments and entry point against the loader’s `boot_args` and physical placement.
- Verify device tree construction, usable-memory reporting, reserved-memory ranges, kernel slide, stack, page
  tables, cache maintenance, and early console output.
- Bring up one CPU first. Then test timer interrupts, GIC, scheduler, and PSCI/secondary CPUs one at a time.
- Validate every board-init function and MMIO mapping. Hard-coded init addresses must not be called until they are
  verified and correctly mapped.

### Gate D — minimum boot milestones

Record reproducible logs and pass in order:

1. UEFI starts and reports the exact board/firmware/DTB.
2. Loader receives and validates the payload; it never selects a redwood or BCM2837 artifact.
3. XNU entry is reached and early UART/loader logging continues.
4. XNU initializes memory, exceptions, GIC, timer, and one CPU without a panic.
5. XNU returns control safely to the resident loader after a bounded test interval.
6. Storage and a read-only root image work.
7. The required iOS userland files are present, compatible, and reach `launchd`, then SpringBoard.

Do not start with write-enabled UFS, display power sequencing, GPU firmware, or all CPU cores. Add those after the
read-only and recovery milestones are repeatable.

### Gate E — image and recovery review

Before any write/boot procedure is documented, it must state:

- Exact model, SoC, bootloader state, firmware/build, CSC, and partition table.
- Exact artifact hashes, format, signing status, size, target partition/temporary-boot method, and expected
  bootloader response.
- The data-loss and Knox effects.
- A tested path back to the matching stock firmware.
- A known failure/timeout exit method.

Only after all of that is reviewed should someone add a command that actually transfers or boots HackiOS on the
S8.

## 8. Recovery and support notes

- Keep the phone’s exact stock firmware and the documented restore method available offline.
- If the phone only reaches Download Mode, stop experimenting and restore only with firmware for the exact model
  and bootloader revision. Do not improvise with another model’s PIT or boot image.
- Samsung Smart Switch can create a data backup, but it is not a substitute for a device-specific bootloader
  recovery plan.
- The official Android Platform Tools documentation explains `adb`/`fastboot`; it does not certify that Samsung
  implements Android fastboot booting.
- Heimdall’s upstream documentation describes communicating with Samsung Download Mode over the Odin protocol;
  it does not validate this project’s image or partition mapping.
- Samsung Knox documentation warns that an unauthorized kernel/boot chain may permanently trip the warranty bit
  and disable Knox-dependent features.

## 9. Local project references

- `README.md` — describes the project’s currently supported redwood/QEMU flows and build assumptions.
- `DREAMLTE_PORTING_GUIDE.md` and `DREAMLTE_PORTING_SUMMARY.md` — earlier experimental notes. Treat their
  addresses and “fastboot boot” instructions as unverified; this guide supersedes them for physical-device use.
- `common/devices/dreamlte.conf` — experimental device profile.
- `common/devices/dreamlte.platform.c` — platform reference fragment; the corresponding platform entry is also
  present in `xnuloader/loader.c`, but its hardware values are not validated.
- `xnu-src/xnu-8796.121.2/config/MASTER.arm64.dreamlte` — XNU build configuration.
- `xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/` — host build outputs; none should be sent to the
  phone without the gates above.

