## @file
#
#  Copyright (c) 2011-2015, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2016, Intel Corporation. All rights reserved.
#  Copyright (c) 2018 - 2019, Bingxing Wang. All rights reserved.
#  Copyright (c) 2022, Xilin Wu. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################

[Defines]
  SOC_PLATFORM            = mt6789
  USE_PHYSICAL_TIMER      = TRUE

!include MediaTekPkg/MediaTekCommonDsc.inc

[PcdsFixedAtBuild.common]
  gArmTokenSpaceGuid.PcdArmArchTimerFreqInHz|13000000
  gArmTokenSpaceGuid.PcdArmArchTimerSecIntrNum|13
  gArmTokenSpaceGuid.PcdArmArchTimerIntrNum|14
  gArmTokenSpaceGuid.PcdArmArchTimerVirtIntrNum|11
  gArmTokenSpaceGuid.PcdArmArchTimerHypIntrNum|10
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x0c000000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x0c040000
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemRevision|0x00000850

  gMediaTekTokenSpaceGuid.PcdMipiFrameBufferLayers|3

  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|2

[LibraryClasses.common]
  KeypadDeviceImplLib|MT6789Pkg/Library/KeypadDeviceImplLib/KeypadDeviceImplLib.inf
  PlatformPeiLib|MT6789Pkg/Library/PlatformPeiLib/PlatformPeiLib.inf
  PlatformPrePiLib|MT6789Pkg/Library/PlatformPrePiLib/PlatformPrePiLib.inf
  MtkGpioImplLib|MT6789Pkg/Library/MtkGpioImplLib/MtkGpioImplLib.inf
  PmicWrapperImplLib|MT6789Pkg/Library/PmicWrapperImplLib/PmicWrapperImplLib.inf
  MsdcPlatformImplLib|MT6789Pkg/Library/MsdcPlatformImplLib/MsdcPlatformImplLib.inf
  RealTimeClockLib|MediaTekPkg/Library/MT6358RtcLib/MT6358RtcLib.inf

[Components.common]
  MT6789Pkg/Drivers/ClockDxe/ClockDxe.inf
  MT6789Pkg/Drivers/MsdcPlatformDxe/MsdcPlatformDxe.inf
  MediaTekPkg/Drivers/MtkGpioDxe/MtkGpioDxe.inf
  MediaTekPkg/Drivers/MT6358PmicDxe/MT6358PmicDxe.inf
  EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf {
    <LibraryClasses>
	  RealTimeClockLib|MediaTekPkg/Library/MT6358RtcLib/MT6358RtcLib.inf
  }
