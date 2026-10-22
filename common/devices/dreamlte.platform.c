/*
 * dreamlte: paste into xnuloader/loader.c next to platform_redwood and add &platform_dreamlte to platforms[].
 * Every value is explained in docs/PORTING.md ("The loader").
 *
 * Samsung Galaxy S8 (dreamlte) with Exynos 8895
 * Exynos 8895: 4x M2 (2.3 GHz) + 4x Cortex-A53 (1.7 GHz), Mali-G71 MP20
 * GICv3, PSCI, UFS storage
 */
static const struct platform platform_dreamlte = {
	.name = "dreamlte",
	/* io_base must be non-zero: XNU treats a zero SoC base as "no SoC" and skips clock setup.
	 * Exynos 8895 peripheral base is typically at 0x10000000 or similar.
	 * This needs to cover the SoC's peripheral region - adjust based on device tree. */
	.io_base = 0x10000000, .io_size = 0x30000000,

	/* GICv3 addresses - from device tree arm,gic-v3 node reg property.
	 * Typical Exynos GIC: distributor at 0x2C000000, redistributor at 0x2C080000 (example) */
	.gic_dist = 0x2C000000, .gic_redist = 0x2C080000,

	.uart = 0,                          /* no PL011 on Exynos - uses Samsung UART */
	.log_other_cpu = 1,

	/* 8 cores: 4x M2 (big) + 4x Cortex-A53 (LITTLE)
	 * Exynos uses MPIDR affinity level 0 for core ID within cluster, level 1 for cluster */
	.cores = 8, .core_shift = 0,

	/* Small cores: the 4x Cortex-A53 (LITTLE cluster) - cores 0-3 typically.
	 * This needs verification from device tree capacity-dmips-mhz */
	.small_cores = 0x0f,

	/* Framebuffer base - the address UEFI GOP reports.
	 * This identifies the device to the loader. Adjust based on actual framebuffer location. */
	.fb_base = 0xE1000000,
};