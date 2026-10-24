# Galaxy S8 Exynos (dreamlte) flashing / recovery

**Do not flash the experimental EDK2/XNU artifacts to device partitions yet.** The current tree does not provide a validated Odin package, Samsung signing/authentication chain, partition map for the target handset, or tested boot chain. A wrong BL/PIT write can hard-brick the phone. `fastboot` is not the Samsung Download Mode flashing interface.

## Download Mode and Heimdall

1. Identify the exact model (SM-G950F/FD/N/etc.) and carrier/region. Do not mix Exynos and Snapdragon firmware or PIT files.
2. Back up data and obtain the matching stock firmware and recovery procedure first. Download Mode normally uses **Volume Down + Bixby + Power** while connecting USB; confirm the on-screen prompt before proceeding.
3. Heimdall communicates with the Samsung Download Mode protocol. First use `heimdall detect` and `heimdall print-pit --no-reboot` to verify connection and inspect partition names. Never use a PIT from another model.
4. Only flash an artifact whose format, target partition, signature requirements, and recovery path have been validated for that exact model. Typical Heimdall syntax is `heimdall flash --PARTITION image-file.img`; the uppercase option must be the exact PIT partition name. **Do not copy/paste a BL/AP/PIT command for this prototype.**
5. Odin is Samsung's Windows service tool; load only a model-matched, correctly packaged firmware file in the intended slot. Do not enable repartition or use a PIT unless a verified device-specific procedure explicitly requires it.
6. Verify boot, recovery, and download-mode recovery before attempting further changes. Keep stock firmware available.

The current deliverable is source/build scaffolding, not a flash-ready image. No partition write should be attempted until boot-chain and recovery are tested on a sacrificial device.
