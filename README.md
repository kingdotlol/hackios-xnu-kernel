# HackiOS on Samsung Galaxy S8 (dreamlte) — XNU Kernel + iOS 16.5 Userland

**Status: experimental — host-only kernel build. Do not flash to the phone yet.**

This repository contains an experimental Exynos 8895 / XNU configuration for the Samsung Galaxy S8 (codename `dreamlte`, Exynos variants only), plus the companion userland repository (`hackios-xnu-userland-dreamlte`, sibling directory) which provides the iOS 16.5 root/system images. The XNU kernel builds to an ARM64 Mach-O executable, but there is **no validated Samsung boot path, no UEFI/DTB/firmware stack, and no flashable image** in either tree.

See **docs/PHYSICAL_S8_GUIDE.md** for the full bring-up guide, device identification steps, and the five engineering gates (A–E) that must be completed before a physical test.

This README is the step-by-step guide for the work that can be done now: prepare the host, identify the phone, link the userland repo, fetch iOS 16.5, build the kernel and userland images, and then stop at the hard stop before anything touches the phone.

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
| `hackios-xnu-kernel` | `/Users/radumoise/Desktop/hackios-xnu-kernel` | XNU kernel patches, EFI loader (`xnuloader`), UEFI port (`edk2-exynos`), build scripts, `xl` phone tool |
| `hackios-xnu-userland-dreamlte` | `/Users/radumoise/Desktop/hackios-xnu-userland-dreamlte` | iOS 16.5 userland: `init`, injected libraries, launchd jobs, APFS image builder, `ios165/fetch.sh` |

The userland repo is checked out into the same directory by `common/setup.sh` (git dir `.userland.git`). Scripts in both repos refer to each other by path, so they must sit side by side.

---

## Step 0 — Prepare the host

Commands below are for macOS. The XNU build was developed on macOS with Xcode; some steps (UEFI/EDK2) may be easier on Linux. Do not mix host instructions blindly.

Check the Apple toolchain:

```sh
cd /Users/radumoise/Desktop/hackios-xnu-kernel
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
cd /Users/radumoise/Desktop/hackios-xnu-kernel
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
cd /Users/radumoise/Desktop/hackios-xnu-userland-dreamlte
ios165/fetch.sh
```

This is a one-time download. It produces `ios165/rootfs`, `ios165/cryptex`, `ios165/cryptex-app`, and `ios165/system.manifest` used by the image builder.

---

## Step 5 — Build the kernel, loader, and userland images

From the kernel repository root, build everything for dreamlte:

```sh
cd /Users/radumoise/Desktop/hackios-xnu-kernel
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
- `common/build.sh dreamlte` — Full build: kernel, loader, images, boot image.
- `common/run.sh dreamlte` — Phone boot via `fastboot boot` (not yet valid for S8).
- `ios165/fetch.sh` — Downloads iOS 16.5 from Apple's CDN (userland repo).
- `userland/build.sh` — Builds `root.apfs` and `system.apfs` (userland repo).

---

## License

This repository is licensed under the [Apple Public Source License 2.0](LICENSE). Our changes to XNU are a derivative of XNU and must be; other code here (loader, scripts) is released under the same license. Third-party code keeps its own license. The userland repository is MIT-licensed; no Apple files are included in either repository — `ios165/fetch.sh` downloads the iOS 16.5 IPSW from Apple's CDN to your machine only.

HackiOS is an independent research project by OOSEX GANG and is not affiliated with, authorised or endorsed by Apple Inc. or Samsung Electronics.
