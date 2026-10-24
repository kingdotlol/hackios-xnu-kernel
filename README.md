# HackiOS on Samsung Galaxy S8 (dreamlte) — XNU Kernel

**Status: experimental — host-only kernel build. Do not flash to the phone yet.**

This repository contains an experimental Exynos 8895 / XNU configuration for the Samsung Galaxy S8 (codename `dreamlte`, Exynos variants only). The XNU kernel builds to an ARM64 Mach-O executable, but there is **no validated Samsung boot path, no UEFI/DTB/firmware stack, and no flashable image** in this tree.

See [docs/PHYSICAL_S8_GUIDE.md](docs/PHYSICAL_S8_GUIDE.md) for the full bring-up guide, device identification steps, and the five engineering gates (A–E) that must be completed before a physical test.

---

## Hardware Target

| Property | Value |
|---|---|
| **Device** | Samsung Galaxy S8 (Exynos variants only) |
| **Codename** | `dreamlte` |
| **SoC** | Samsung Exynos 8895 (4× M2 @ 2.3 GHz + 4× Cortex-A53 @ 1.7 GHz) |
| **GPU** | ARM Mali-G71 MP20 (20 shader cores, Midgard) |
| **Boot path** | Samsung Download Mode (Odin/Loke) — **not** Android `fastboot boot` |

**Verify your device before proceeding:**
```sh
adb shell getprop ro.product.device    # must be "dreamlte"
adb shell getprop ro.boot.hardware     # must be "dreamlte"
adb shell cat /proc/cpuinfo            # must show Exynos 8895 cores
```
Do not assume an `SM-G950*` model suffix alone proves the SoC.

---

## What's in This Tree for dreamlte

- `xnu/files/bsd/dev/dreamlte_gpu.c` — Mali-G71 initialization, power management, register access
- `xnu/files/bsd/sys/dreamlte_gpu.h` — Kernel interfaces for GPU, MMU, job submission, fences, compute
- `common/devices/dreamlte.conf` — Experimental device profile (UEFI_FD and DTB currently blank)
- `common/devices/dreamlte.platform.c` — Loader platform reference fragment (addresses unverified)
- `xnu-src/xnu-8796.121.2/config/MASTER.arm64.dreamlte` — XNU build configuration
- `uefi/files/Platforms/Samsung/dreamltePkg` — UEFI port directory (no FD/DTB supplied)

---

## Host-Only Kernel Build (macOS with Xcode toolchain)

```sh
cd ~/hackios-xnu-kernel/xnu-src/xnu-8796.121.2

SDKROOT="$(xcrun --sdk iphoneos --show-sdk-path)"
make SDKROOT="$SDKROOT" \
  PLATFORM=iPhoneOS \
  ARCH_STRING_FOR_CURRENT_MACHINE_CONFIG=arm64 \
  ARCH_CONFIGS=ARM64 \
  MACHINE_CONFIGS=DREAMLTE \
  KERNEL_CONFIGS=DEVELOPMENT \
  2>&1 | tee ~/dreamlte-ios-build.log
```

Verify output:
```sh
KERNEL=BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped
test -s "$KERNEL" && file "$KERNEL" && xcrun dwarfdump --uuid "$KERNEL"
```

**Expected:** ARM64 Mach-O executable.  
**This is not a flashable Samsung image.** It proves only that the host linker emitted a file; it does not validate the boot path, memory map, interrupt setup, or UEFI handoff.

---

## Current Hard Stop: Do Not Flash

There is **no supported physical-S8 install command** for this checkout.

Specifically unsafe/incorrect right now:
```sh
# DO NOT RUN:
fastboot boot out/dreamlte-boot.img      # fastboot is not the S8 entry path
heimdall flash --BOOT ...                # no validated image exists
```

Reasons:
- No verified `out/dreamlte-boot.img` exists from the current path
- XNU output is Mach-O, not a Samsung Download Mode package
- `fastboot boot` is wired for a different supported phone (redwood)
- No tested Samsung bootloader/UEFI handoff, no validated image header/container
- Wrong partition/model/PIT/bootloader image could leave the device unbootable

---

## Engineering Gates Required Before Physical Test

Require written review and evidence for each gate:

| Gate | Requirement |
|---|---|
| **A — Exact hardware description** | Obtain DTB/DTBO and firmware for exact S8 model/region. Verify every memory/peripheral address, RAM size, CPU topology, GIC, UART, timer, UFS, display, I2C against device tree. Replace guessed addresses. |
| **B — Working loader/firmware path** | Build correct Samsung/Exynos UEFI image with real S8 firmware binaries and matching DTB. Register DREAMLTE platform in loader, verify detection. Define temporary load/restore procedure; test with harmless recovery image first. |
| **C — XNU platform handoff** | Verify Mach-O segments/entry point against loader `boot_args`. Validate device tree, usable memory, reserved ranges, kernel slide, page tables, cache maintenance, early console. Bring up one CPU, then timer, GIC, scheduler, PSCI/secondaries. |
| **D — Minimum boot milestones** | 1) UEFI reports exact board/firmware/DTB 2) Loader validates payload (never selects redwood/BCM2837) 3) XNU entry reached, early UART works 4) Memory, exceptions, GIC, timer, one CPU init without panic 5) XNU returns to loader after bounded test 6) Storage + read-only root image work 7) iOS userland reaches SpringBoard |
| **E — Image and recovery review** | Document exact model, SoC, bootloader state, firmware, CSC, partition table, artifact hashes, format, signing, target partition, data-loss/Knox effects, tested restore path, failure exit method. |

---

## Recovery Notes

- Keep exact stock firmware and documented restore method available offline
- If phone only reaches Download Mode: restore only with firmware for exact model and bootloader revision. Do not improvise with another model's PIT or boot image.
- Samsung Knox: unauthorized kernel/boot chain may permanently trip warranty bit and disable Knox features.

---

## Local References

- `docs/PHYSICAL_S8_GUIDE.md` — Full bring-up guide (this document's source)
- `common/devices/dreamlte.conf` — Experimental device profile
- `common/devices/dreamlte.platform.c` — Loader platform fragment (unverified)
- `xnu-src/xnu-8796.121.2/config/MASTER.arm64.dreamlte` — XNU build config
- `xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/` — Host build outputs (do not send to phone without gates above)

---

## License

This repository is licensed under the [Apple Public Source License 2.0](LICENSE). Our changes to XNU are a derivative of XNU and must be; other code here (loader, scripts) is released under the same license. Third-party code keeps its own license.

---

**OOSEX GANG** — [@oodze](https://t.me/oodze) (founder), [@netusd](https://t.me/netusd), [@wannqn](https://t.me/wannqn), [@hxfuxyyXD](https://t.me/hxfuxyyXD). News: [t.me/oosexgangdevs](https://t.me/oosexgangdevs).