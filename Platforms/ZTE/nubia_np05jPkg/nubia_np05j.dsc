##
#  Copyright (c) 2011 - 2022, ARM Limited. All rights reserved.
#  Copyright (c) 2014, Linaro Limited. All rights reserved.
#  Copyright (c) 2015 - 2020, Intel Corporation. All rights reserved.
#  Copyright (c) 2018, Bingxing Wang. All rights reserved.
#  Copyright (c) Microsoft Corporation.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
##

################################################################################
#
# Defines Section - statements that will be processed to create a Makefile.
#
################################################################################
[Defines]
  PLATFORM_NAME                  = nubia_np05j
  PLATFORM_GUID                  = FEE83493-49FB-57BC-B951-74F52963EFCF
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/nubia_np05jPkg
  SUPPORTED_ARCHITECTURES        = AARCH64
  BUILD_TARGETS                  = RELEASE|DEBUG
  SKUID_IDENTIFIER               = DEFAULT
  FLASH_DEFINITION               = nubia_np05jPkg/nubia_np05j.fdf
  # NP05JHW1.0, SoC 639 rev 2.0: six CPUs at 3.5328 GHz and two at 4.32 GHz.
  SOC_TYPE                       = 0
  USE_CUSTOM_DISPLAY_DRIVER      = 1

!include PakalaPkg/PakalaPkg.dsc.inc

[PcdsFixedAtBuild]
  #
  # DDR Memory
  #
  gArmTokenSpaceGuid.PcdSystemMemoryBase|0x80000000

  #
  # UEFI Stack
  #
  gArmPlatformTokenSpaceGuid.PcdCPUCoresStackBase|0xA760D000
  gArmPlatformTokenSpaceGuid.PcdCPUCorePrimaryStackSize|0x40000

  #
  # SMBIOS
  #
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemManufacturer|"nubia"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemModel|"NP05J"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailModel|"REDMAGIC Astra"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemRetailSku|"PQ84P01"
  gSiliciumPkgTokenSpaceGuid.PcdSmbiosSystemBoardModel|"NP05JHW1.0"

  #
  # Early debug console geometry from the selected panel timing.
  #
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferWidth|2400
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferHeight|1504
  gSiliciumPkgTokenSpaceGuid.PcdFrameBufferColorDepth|32

  #
  # Platform PEI
  #
  gQcomPkgTokenSpaceGuid.PcdPlatformType|"LA"

[LibraryClasses]
  #
  # Memory Libraries
  #
  MemoryMapLib|nubia_np05jPkg/Library/MemoryMapLib/MemoryMapLib.inf

  #
  # QCOM Libraries
  #
  ConfigurationMapLib|nubia_np05jPkg/Library/ConfigurationMapLib/ConfigurationMapLib.inf

[Components]
  #
  # ACPI Tables
  #
  nubia_np05j/AcpiTables.inf

!if $(TARGET) == DEBUG
  # Standalone diagnostic for the first BDS session.
  nubia_np05jPkg/Application/Np05jProbe/Np05jProbe.inf
!endif
