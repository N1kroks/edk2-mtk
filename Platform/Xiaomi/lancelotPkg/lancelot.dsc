[Defines]
  PLATFORM_NAME                  = lancelot
  PLATFORM_GUID                  = e8d9c2fa-20ef-446a-ae3d-46152801c9a8
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010019
  OUTPUT_DIRECTORY               = Build/$(PLATFORM_NAME)
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = DEBUG|RELEASE
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = lancelotPkg/lancelot.fdf
  DEVICE_DXE_FV_COMPONENTS       = lancelotPkg/lancelot.fdf.inc

[PcdsFixedAtBuild.common]
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x40000000
  gArmTokenSpaceGuid.PcdSystemMemorySize|0xC0000000 # 3GB
  gArmTokenSpaceGuid.PcdCpuVectorBaseAddress|0x40000000
  gEmbeddedTokenSpaceGuid.PcdPrePiStackBase|0x40001000
  gEmbeddedTokenSpaceGuid.PcdPrePiStackSize|0x00040000
  gMediaTekTokenSpaceGuid.PcdUefiMemPoolBase|0x40280000
  gMediaTekTokenSpaceGuid.PcdUefiMemPoolSize|0x03C00000

  # Simple Framebuffer
  gMediaTekTokenSpaceGuid.PcdMipiFrameBufferWidth|1088
  gMediaTekTokenSpaceGuid.PcdMipiFrameBufferHeight|2340
  gMediaTekTokenSpaceGuid.PcdMipiFrameBufferAddress|0x7d9b0000

[LibraryClasses]
  PlatformMemoryMapLib|lancelotPkg/Library/PlatformMemoryMapLib/PlatformMemoryMapLib.inf

!include Silicon/MediaTek/MT6768Pkg/MT6768.dsc
