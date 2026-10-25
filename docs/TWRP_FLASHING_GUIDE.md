# HackiOS on Samsung Galaxy S8 (dreamlte) — TWRP Flashing Guide to iOS 16 Home Screen

> **⚠️ CRITICAL WARNING**: This is **experimental research software**. Flashing custom boot code can:
> - Permanently brick the device (hard brick, unrecoverable)
> - Trip Samsung Knox warranty bit (irreversible, disables Knox features forever)
> - Erase all data
> - Require JTAG/EDL mode recovery if bootloader is corrupted
>
> **Only proceed if you accept total loss of the device.** Keep exact stock firmware + tested restore procedure ready offline.

---

## 📋 Prerequisites Checklist

### Hardware
- [ ] **Samsung Galaxy S8 Exynos variant ONLY** (SM-G950F/FD/N — verify `dreamlte` codename)
- [ ] Known-good USB data cable
- [ ] Charged battery (>50%)
- [ ] **Second device** for backup/restore reference

### Software (Host Machine)
- [ ] Linux host (Ubuntu 22.04+ recommended) for UEFI/build
- [ ] macOS host (for XNU kernel build with Xcode + iPhoneOS16.5.sdk)
- [ ] Android Platform Tools (`adb`, `fastboot`)
- [ ] Heimdall (`heimdall-flash`) for Download Mode inspection
- [ ] Odin (Windows) or Heimdall (Linux) for flashing
- [ ] TWRP recovery installed on device (see Step 0)

### Device Verification (MANDATORY — run before ANY flashing)
```bash
adb devices -l
adb shell getprop ro.product.device      # MUST be "dreamlte"
adb shell getprop ro.boot.hardware       # MUST be "dreamlte"
adb shell cat /proc/cpuinfo              # MUST show Exynos 8895 (4× M2 + 4× A53)
```
**If any check fails, STOP. Wrong variant = guaranteed brick.**

---

## 🛠 Step 0: Install TWRP Recovery (Prerequisite)

The S8 uses **Download Mode (Odin)**, not fastboot. You need TWRP first to flash the HackiOS boot image.

### 0.1 Unlock Bootloader (OEM Unlock)
1. Settings → Developer Options → **OEM Unlock: ON**
2. Reboot to Download Mode: `Vol Down + Bixby + Power` → `Vol Up`
3. In Odin: Flash `vbmeta_disabled_R.tar` (disables AVB verification)
   - AP slot: `vbmeta_disabled_R.tar`
   - Options: **Only** "Auto Reboot" + "F. Reset Time"
4. Device reboots → "Device is corrupt" screen → `Vol Up` to continue

### 0.2 Flash TWRP
```bash
# Download TWRP for dreamlte (Exynos S8)
# Source: https://twrp.me/samsung/samsunggalaxys8exynos.html
# File: twrp-3.7.0_9-0-dreamlte.img.tar (or latest)

# In Download Mode (Vol Down + Bixby + Power → Vol Up):
# Odin: AP = twrp-3.7.0_9-0-dreamlte.img.tar
# Options: Auto Reboot + F. Reset Time ONLY
# Click Start
```

### 0.3 Verify TWRP Boots
- Reboot to recovery: `Vol Up + Bixby + Power`
- TWRP main menu should appear
- **Mount** → enable MTP → transfer files via `adb push`

---

## 🔨 Step 1: Build the HackiOS TWRP-Flashable Image

### 1.1 Host Setup (Linux)
```bash
sudo apt-get update && sudo apt-get install -y \
  git python3 python3-pip python3-venv \
  build-essential clang llvm lld \
  gcc-aarch64-linux-gnu nasm uuid-dev \
  flex bison m4 autoconf automake \
  mono-devel p7zip-full \
  libssl-dev pkg-config

pip3 install --user -r Mu-Silicium/pip-requirements.txt
export PATH="$HOME/.local/bin:$PATH"
```

### 1.2 Clone & Setup Repos
```bash
git clone https://github.com/oosexgang/hackios-xnu-kernel.git
cd hackios-xnu-kernel
./common/setup.sh                    # Links userland repo (.userland.git)

cd Mu-Silicium
git submodule update --init --recursive
cd ..
```

### 1.3 Build Mu-Silicium UEFI for dreamlte
```bash
cd Mu-Silicium

# Fix nasm for NuGet SSL issue
mkdir -p Mu_Basecore/BaseTools/Bin/mu_nasm_extdep/Linux-ARM-64
cp /usr/bin/nasm Mu_Basecore/BaseTools/Bin/mu_nasm_extdep/Linux-ARM-64/nasm
echo 'version: 20016.1.1' > Mu_Basecore/BaseTools/Bin/mu_nasm_extdep/extdep_state.yaml

# Build UEFI
python3 build_uefi.py -d dreamlte
cd ..

# Copy to expected locations
mkdir -p edk2-exynos/Build/dreamltePkg/RELEASE_CLANG38/FV
cp Mu-Silicium/Build/dreamltePkg/RELEASE_CLANGPDB/FV/SILICIUM_UEFI.fd \
   edk2-exynos/Build/dreamltePkg/RELEASE_CLANG38/FV/
cp Mu-Silicium/Resources/DTBs/dreamlte.dtb exynos8895-dreamlte.dtb
```

### 1.4 Fetch iOS 16.5 Userland (macOS or Linux)
```bash
# In userland repo (sibling directory)
cd ../hackios-xnu-userland-dreamlte
ln -s ../hackios-xnu-kernel/xnu-deps ./xnu-deps
./ios165/fetch.sh                    # ~7.4 GB download from Apple CDN

# Copy to kernel repo
cp -r ios165/rootfs ios165/cryptex ios165/cryptex-app \
      ios165/20F66__iPhone10,3_6 ios165/system.files \
      ios165/system.manifest ios165/var.skel \
      ../../hackios-xnu-kernel/ios165/
```

### 1.5 Build Loader & Userland Images
```bash
cd /path/to/hackios-xnu-kernel

# Build XNU dependencies
./xnu-deps/setup.sh

# Build loader (UEFI app)
./xnuloader/build.sh

# Build userland APFS images (use prebuilt to avoid linker issues)
cd userland
PREBUILT=1 REST=1 ./build.sh         # Creates root.apfs (~4 MB)
PREBUILT=1 REST=1 SYSTEM=1 ./build.sh # Creates system.apfs (~2.4 GB)
cd ..
```

### 1.6 Create TWRP-Flashable Boot Image
```bash
export PATH="$HOME/.local/bin:$PATH"

# Standard fastboot image (NOT for S8 — reference only)
SKIP="kernel" ./common/build.sh dreamlte
# Output: out/dreamlte-boot.img (688 KB)

# TWRP-FLASHABLE Samsung image (THIS IS WHAT YOU NEED)
python3 mkxnuimg_twrp.py -o out/dreamlte-boot-twrp.img --create-payload
# Output: out/dreamlte-boot-twrp.img (692 KB)
```

### 1.7 Verify Build Outputs
```bash
ls -lh out/dreamlte-boot-twrp.img userland/root.apfs userland/system.apfs
# Expected:
# out/dreamlte-boot-twrp.img    692 KB  (Samsung DTBH header, TWRP/Odin compatible)
# userland/root.apfs              4 MB
# userland/system.apfs           2.4 GB

# Verify kernel built
KERNEL=xnu-src/xnu-8796.121.2/BUILD/obj/DEVELOPMENT_ARM64_DREAMLTE/mach.development.dreamlte.unstripped
test -s "$KERNEL" && file "$KERNEL"
# Should show: "Mach-O 64-bit executable arm64"
```

---

## 📱 Step 2: Transfer Images to Device

```bash
# Push boot image to device (via TWRP MTP or adb)
adb push out/dreamlte-boot-twrp.img /sdcard/
adb push userland/root.apfs /sdcard/
adb push userland/system.apfs /sdcard/

# Verify on device
adb shell ls -lh /sdcard/dreamlte-boot-twrp.img /sdcard/root.apfs /sdcard/system.apfs
```

---

## ⚡ Step 3: Flash via TWRP Recovery

### 3.1 Boot to TWRP
```
Power off → Vol Up + Bixby + Power → hold until TWRP logo
```

### 3.2 Flash HackiOS Boot Image
1. **Install** → **Install Image**
2. Select `/sdcard/dreamlte-boot-twrp.img`
3. Partition: **Boot** (NOT recovery, NOT dtbo)
4. Swipe to confirm flash
5. **Do NOT reboot yet**

### 3.3 Flash Userland Images (APFS volumes)
The iOS userland requires two APFS partitions. These must be flashed to **userdata** partitions or via a custom partition scheme.

> **⚠️ CURRENT LIMITATION**: The project does not yet define a validated partition map for the APFS volumes. The `system.apfs` (2.4 GB) and `root.apfs` (4 MB) need target partitions. Options:
> - **Option A**: Flash `system.apfs` to `userdata` partition (will wipe Android data)
> - **Option B**: Use a custom PIT with dedicated APFS partitions (advanced, risky)
> - **Option C**: Boot to UEFI shell first, then load APFS from USB (not yet implemented)

**For experimental testing, flash system.apfs to userdata:**
```bash
# In TWRP terminal (Advanced → Terminal):
dd if=/sdcard/system.apfs of=/dev/block/by-name/userdata bs=4M status=progress
# WARNING: This DESTROYS Android userdata partition
```

---

## 🔄 Step 4: Boot Process — What Happens

### Expected Boot Sequence (if everything works)

| Stage | Component | Expected Behavior |
|-------|-----------|-------------------|
| 1 | **Samsung Boot ROM** | Loads BL1 → BL2 → SBOOT (stock) |
| 2 | **SBOOT** | Verifies boot image signature (may fail — Knox trip) |
| 3 | **HackiOS Boot Image** | Android boot header v1 → loads kernel + DTB |
| 4 | **BootShim** | Decompresses UEFI FD + XNUP payload to RAM |
| 5 | **Mu-Silicium UEFI** | Initializes Exynos 8895 hardware, loads DTB |
| 6 | **XNU Loader (loader.efi)** | Parses DTB, builds `boot_args`, loads XNU kernel |
| 7 | **XNU Kernel (iOS 16.5)** | Mach-O entry → early init → UART console |
| 8 | **iOS Userland** | mount `root.apfs` → `launchd` → `SpringBoard` |

### Known Failure Points (Current State)
| Gate | Status | Likely Failure |
|------|--------|----------------|
| A: Hardware Description | ❌ Unverified | Wrong memory map, DTB mismatch |
| B: UEFI/Loader Path | ❌ Untested | UEFI FD not matched to exact S8 firmware |
| C: XNU Handoff | ❌ Unvalidated | `boot_args` mismatch, no early UART |
| D: Boot Milestones | ❌ Not reached | Kernel panic, no display, no storage |
| E: Recovery Review | ❌ Incomplete | No tested restore path |

**Most likely outcome**: Device shows black screen, bootloop, or "Custom Binary Blocked" / Knox trip screen.

---

## 🆘 Step 5: Recovery Procedures

### 5.1 If Stuck in Bootloop / Black Screen
1. Force Download Mode: `Vol Down + Bixby + Power` (hold 10s) → `Vol Up`
2. Flash **stock firmware** via Odin (AP/BL/CP/CSC from same firmware package)
3. **Do NOT** use "Repartition" or custom PIT

### 5.2 If Only Download Mode Works
```bash
# Linux: Heimdall restore (read PIT first!)
heimdall print-pit --no-reboot > pit_backup.txt
heimdall flash --AP stock_AP.tar.md5 --BL stock_BL.tar.md5 \
               --CP stock_CP.tar.md5 --CSC stock_CSC.tar.md5
```

### 5.3 If Knox Tripped (0x1 warranty bit)
- **Irreversible**. Samsung Pay, Secure Folder, Knox attestation permanently disabled.
- Device still boots stock Android but loses enterprise features.

### 5.4 Emergency Exit from Download Mode (No Write in Progress)
```
Hold Vol Down + Power for ~7 seconds → device reboots normally
```

---

## 📊 Step 6: Debugging & Logs

### 6.1 Get Early Boot Logs (if UART works)
- Hardware UART on S8: **Headphone jack (TRRS) or USB-C debug cable**
- Requires custom kernel cmdline: `uart=0x38300000` (unverified address)

### 6.2 Check TWRP Flash Log
```bash
# In TWRP: Advanced → Copy Log → /sdcard/recovery.log
adb pull /sdcard/recovery.log
```

### 6.3 Verify Image Integrity
```bash
# On host
sha256sum out/dreamlte-boot-twrp.img
# Compare with expected from build

# Check DTBH header in image
xxd -s 0x6f000 -l 64 out/dreamlte-boot-twrp.img
# Should show "DTBH" magic at offset ~0x6f000
```

---

## ✅ Step 7: Success Criteria (iOS 16 Home Screen)

You have reached the goal when **ALL** are true:

| Milestone | Verification |
|-----------|--------------|
| ☐ UEFI banner | "SILICIUM UEFI" on screen / UART |
| ☐ Loader runs | "XNU Loader: payload validated" log |
| ☐ XNU entry | "Darwin Kernel Version 21.x" (iOS 16 = Darwin 21) |
| ☐ Early init | No panic, UART scroll continues |
| ☐ root.apfs mount | "APFS volume mounted at /" |
| ☐ launchd starts | "launchd[1]: Bootstrap complete" |
| ☐ SpringBoard | **iOS 16 Lock Screen → Home Screen** 🎉 |

---

## 📝 Appendix: Image Structure Reference

```
out/dreamlte-boot-twrp.img (Android Boot Image v1, 692 KB)
├── Header (2048 bytes)
├── Kernel = gzip(BootShim + UEFI FD + XNUP payload) + DTB (DTBH header)
├── Ramdisk = 5-byte placeholder
└── DTB (appended to kernel, with Samsung DTBH header)

Memory Map (Exynos 8895, from mkxnuimg_twrp.py):
  BASE_ADDR     = 0x10000000
  KERNEL_ADDR   = 0x10008000  (kernel offset 0x8000)
  RAMDISK_ADDR  = 0x11000000  (ramdisk offset 0x1000000)
  DTB_ADDR      = 0x10000000  (DTB offset 0x0)
  PAYLOAD_BASE  = 0xF0000000  (XNUP payload in high mem)
  FD_BASE       = 0x7FD00000  (UEFI FD)
  FD_SIZE       = 0x00200000  (2 MB)
```

---

## ⚖️ Legal & Ethical Notice

- **HackiOS** is an independent research project by OOSEX GANG
- Not affiliated with, authorized, or endorsed by **Apple Inc.** or **Samsung Electronics**
- iOS 16.5 IPSW downloaded from Apple CDN for research only
- Licensed under **Apple Public Source License 2.0** (XNU) + **MIT** (userland scripts)
- You assume all liability for device damage, data loss, Knox trip, or legal consequences

---

## 📚 Reference Files in This Repo

| File | Purpose |
|------|---------|
| `README.md` | Full build guide (host-only) |
| `docs/PHYSICAL_S8_GUIDE.md` | Engineering gates A–E, safety procedures |
| `docs/DREAMLTE_FLASHING.md` | Download Mode / Heimdall / Odin reference |
| `mkxnuimg_twrp.py` | Creates TWRP-flashable image with DTBH header |
| `common/devices/dreamlte.conf` | Device profile (addresses unverified) |
| `xnuloader/loader.c` | Platform detection + boot_args builder |
| `Mu-Silicium/` | UEFI port for Exynos 8895 |

---

**Final reminder**: This guide documents the *current experimental procedure* from the HackiOS source tree. The project explicitly states **"Do not flash to the phone yet"** and **"There is no supported physical-S8 install command."** You are proceeding at your own risk on a known-incomplete boot chain.

**If you reach SpringBoard — document everything and share with the project. That would be a historic milestone.**