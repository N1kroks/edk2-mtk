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
  SOC_PLATFORM            = mt6768
  USE_PHYSICAL_TIMER      = TRUE
  HAS_BROKEN_TIMER        = FALSE

!include MediaTekPkg/MediaTekCommonDsc.inc

[PcdsFixedAtBuild.common]
  gArmTokenSpaceGuid.PcdArmArchTimerFreqInHz|13000000
  gArmTokenSpaceGuid.PcdArmArchTimerSecIntrNum|29
  gArmTokenSpaceGuid.PcdArmArchTimerIntrNum|30
  gArmTokenSpaceGuid.PcdArmArchTimerVirtIntrNum|27
  gArmTokenSpaceGuid.PcdArmArchTimerHypIntrNum|26
  gArmTokenSpaceGuid.PcdGicDistributorBase|0x0c000000
  gArmTokenSpaceGuid.PcdGicRedistributorsBase|0x0c040000
  gArmTokenSpaceGuid.PcdGicInterruptInterfaceBase|0x0c040000
  gEfiMdeModulePkgTokenSpaceGuid.PcdAcpiDefaultOemRevision|0x00000850

  gArmPlatformTokenSpaceGuid.PcdCoreCount|8
  gArmPlatformTokenSpaceGuid.PcdClusterCount|2

[LibraryClasses.common]
  KeypadDeviceImplLib|MT6768Pkg/Library/KeypadDeviceImplLib/KeypadDeviceImplLib.inf
  PlatformPeiLib|MT6768Pkg/Library/PlatformPeiLib/PlatformPeiLib.inf
  PlatformPrePiLib|MT6768Pkg/Library/PlatformPrePiLib/PlatformPrePiLib.inf
  MtkGpioImplLib|MT6768Pkg/Library/MtkGpioImplLib/MtkGpioImplLib.inf
  PmicWrapperImplLib|MT6768Pkg/Library/PmicWrapperImplLib/PmicWrapperImplLib.inf

[Components.common]
  MediaTekPkg/Drivers/MtkGpioDxe/MtkGpioDxe.inf
  MediaTekPkg/Drivers/MT6358PmicDxe/MT6358PmicDxe.inf
  EmbeddedPkg/RealTimeClockRuntimeDxe/RealTimeClockRuntimeDxe.inf {
    <LibraryClasses>
	  RealTimeClockLib|MediaTekPkg/Library/MT6358RtcLib/MT6358RtcLib.inf
  }
