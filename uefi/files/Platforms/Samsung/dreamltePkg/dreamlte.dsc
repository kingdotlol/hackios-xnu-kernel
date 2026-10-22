[Defines]
  DSC_SPECIFICATION              = 0x00010005
  PACKAGE_NAME                   = dreamltePkg
  PACKAGE_GUID                   = 7a6f3b2c-4d5e-6f7a-8b9c-0d1e2f3a4b5c
  PACKAGE_VERSION                = 1.0

[LibraryClasses.common]
  MemoryMapLib|Platforms/Samsung/dreamltePkg/Library/MemoryMapLib/MemoryMapLib.inf
  ConfigurationMapLib|Platforms/Samsung/dreamltePkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf

[Components]
  Platforms/Samsung/dreamltePkg/Library/MemoryMapLib/MemoryMapLib.inf
  Platforms/Samsung/dreamltePkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf

[PcdsFixedAtBuild.common]
  gEfiMdePkgTokenSpaceGuid.PcdDebugPropertyMask|0x00
  gEfiMdePkgTokenSpaceGuid.PcdDebugPrintErrorLevel|0x80000000
  gEfiMdePkgTokenSpaceGuid.PcdMaximumUnicodeStringLength|1000000

[BuildOptions.common]
  MSFT:*_*_CC_FLAGS = /O2 /GL /W4 /WX- /Zi /Gy /GF /Gm- /EHsc /MD /GS /fp:precise /Zc:wchar_t /Zc:forScope /Zc:inline /GR- /DUNICODE /D_UNICODE
  GCC:*_*_CC_FLAGS = -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter -Wno-missing-field-initializers

[Components.common]
  Silicon/Silicium/SiliciumPkg/Library/BootManagerLib/BootManagerLib.inf

[PcdsFeatureFlag.common]
  gEfiMdePkgTokenSpaceGuid.PcdDriverDiagnostics2Support|FALSE
  gEfiMdePkgTokenSpaceGuid.PcdComponentName2Support|FALSE