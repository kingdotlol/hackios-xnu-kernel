#include <Library/ConfigurationMapLib.h>

STATIC
EFI_CONFIGURATION_ENTRY_DESCRIPTOR
gConfigurationDescriptor[] = {
  // Configuration Map for Exynos 8895 (dreamlte)

  // CPU configuration
  {"NumCpus", 8},
  {"NumActiveCores", 8},
  {"MaxCoreCnt", 8},
  {"EarlyInitCoreCnt", 2},
  {"EnableMultiThreading", 1},
  {"EnableMultiCoreFvDecompression", 1},

  // Memory configuration
  {"InitialPagePoolCount", 0x800},
  {"UefiMemUseThreshold", 0xE1},
  {"MaxLogFileSize", 0x400000},
  {"ShmBridgememSize", 0xA00000},

  // UART/Debug configuration
  {"SharedIMEMBaseAddr", 0x02040000},
  {"DloadCookieAddr", 0x02041000},
  {"DloadCookieValue", 0x10},

  // Security
  {"SecurityFlag", 0xC4},
  {"EnableVariablePolicyEngine", 0},
  {"AllowNonPersistentVarsInRetail", 0x1},

  // USB configuration
  {"USBHS1_Config", 0x0},
  {"UsbFnIoRevNum", 0x00010001},

  // Storage
  {"EnableSDHCSwitch", 0x1},
  {"EnableUfsIOC", 1},
  {"UfsSmmuConfigForOtherBootDev", 1},
  {"UEFIExitUfsSSURequired", 1},

  // Display
  {"EnableDisplayThread", 0x0},
  {"EnableDisplayImageFv", 0x1},

  // Power/Reset
  {"PwrBtnShutdownFlag", 0x0},
  {"DetectRetailUserAttentionHotkey", 0x00},
  {"DetectRetailUserAttentionHotkeyCode", 0x17},
  {"EnableOEMSetupAppInRetail", 0x0},
  {"EnableLogFsSyncInRetail", 0x1},

  // Debug
  {"EnableUefiSecAppDebugLogDump", 0x0},
  {"EnableACPIFallback", 0x0},
  {"DDRInfoNotifyFlag", 0x0},

  // Exynos specific
  {"MinidumpTALoadingCfg", 0x0},
  {"EUDEnableAddr", 0x0},
  {"NumCpusFuseAddr", 0x0},
};

VOID
GetConfigurationMap (
  OUT EFI_CONFIGURATION_ENTRY_DESCRIPTOR **ConfigurationDescriptor,
  OUT UINT8                               *ConfigurationDescriptorCount)
{
  // Pass Data
  *ConfigurationDescriptor      = gConfigurationDescriptor;
  *ConfigurationDescriptorCount = ARRAY_SIZE (gConfigurationDescriptor);
}