# ExynosMali boundary (not a functioning kbase port)

This XNU checkout has no IOKit/KEXT runtime. `ExynosMali.cpp` is therefore an
unregistered C++ seam and returns `ENOTSUP`; it does not submit GPU jobs.
`kbase_shim.h` defines only a small ABI boundary, not ARM's proprietary kbase.

Before enabling: verify the actual Mali physical base (the S8 Exynos 8895 GPU
is generally documented at 0x18000000; the existing `dreamlte_gpu.c` header and
port notes must agree), interrupt number/trigger, clock/reset/power sequencing,
SMMU stream IDs, firmware provenance, GPU VA page tables, cache coherency, and
DMA constraints against device data and Linux source. Implement pin/unpin and
IOMMU isolation before exposing any user interface. Do not wire in guessed
register programming or claim OpenGL/Metal support. The current BSD driver is
still only early register probing/power scaffolding.
