#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/PrintLib.h>

#include <Protocol/EFIClock.h>
#include <Protocol/EFIDDRGetConfig.h>
#include <Protocol/EFIChipInfo.h>

#include "SmBiosTable.h"

//
// Global Protocols
//
STATIC EFI_CLOCK_PROTOCOL      *mClockProtocol    = NULL;
STATIC EFI_CHIPINFO_PROTOCOL   *mChipInfoProtocol = NULL;
STATIC EFI_DDRGETINFO_PROTOCOL *mDdrInfoProtocol  = NULL;

STATIC_ASSERT (
  OFFSET_OF (EFI_CLOCK_PROTOCOL, GetCpuPerfLevelFrequency64) == 0x110,
  "EFI_CLOCK_PROTOCOL::GetCpuPerfLevelFrequency64 offset mismatch"
  );

STATIC
EFI_STATUS
GetClusterPerfLevelFreqMHz (
  IN  UINT32  Cluster,
  IN  UINT32  PerformanceLevel,
  OUT UINT16 *FreqMHz)
{
  EFI_STATUS Status;

  if (mClockProtocol->Version >= EFI_CLOCK_PROTOCOL_VERSION_64BIT &&
      mClockProtocol->GetCpuPerfLevelFrequency64 != NULL)
  {
    UINT64 FreqHz          = 0;
    UINT64 RequiredVoltage = 0;

    Status = mClockProtocol->GetCpuPerfLevelFrequency64 (
               mClockProtocol,
               Cluster,
               PerformanceLevel,
               &FreqHz,
               &RequiredVoltage
               );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    *FreqMHz = (UINT16)(FreqHz / 1000000ULL);
    return EFI_SUCCESS;
  }

  UINT32 FreqHz          = 0;
  UINT32 RequiredVoltage = 0;

  Status = mClockProtocol->GetCpuPerfLevelFrequency (
             mClockProtocol,
             Cluster,
             PerformanceLevel,
             &FreqHz,
             &RequiredVoltage
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  *FreqMHz = (UINT16)(FreqHz / 1000000U);
  return EFI_SUCCESS;
}

EFI_STATUS
GetClusterSpeeds (
  IN  UINT32  Cluster,
  OUT UINT16 *MaxSpeed,
  OUT UINT16 *CurrentSpeed)
{
  EFI_STATUS Status;
  UINT32     MaxPerfLevel     = 0;
  UINT32     CurrentPerfLevel = 0;
  UINT16     MaxFreqMHz       = 0;
  UINT16     CurrentFreqMHz   = 0;

  // Verify Protocol Presense
  if (mClockProtocol == NULL) {
    return EFI_NOT_READY;
  }

  // Verify Protocol Revision
  if (mClockProtocol->Version < 0x10006) {
    return EFI_UNSUPPORTED;
  }

  // Get Max Performance Level
  Status = mClockProtocol->GetMaxPerfLevel (mClockProtocol, Cluster, &MaxPerfLevel);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Convert Max Performance Level to MHz
  Status = GetClusterPerfLevelFreqMHz (Cluster, MaxPerfLevel, &MaxFreqMHz);
  if (EFI_ERROR (Status) && MaxPerfLevel > 0) {
    // Newer ClockDxe (e.g. Pakala) returns level count N from GetMaxPerfLevel
    // while valid performance level indices are [0 .. N - 1].
    Status = GetClusterPerfLevelFreqMHz (Cluster, MaxPerfLevel - 1, &MaxFreqMHz);
  }
  if (EFI_ERROR (Status) || MaxFreqMHz == 0) {
    return EFI_ERROR (Status) ? Status : EFI_DEVICE_ERROR;
  }

  // Get Current Performance Level (fall back to MaxSpeed when SCMI LevelGet is unsupported)
  Status = mClockProtocol->GetCpuPerfLevel (mClockProtocol, Cluster, &CurrentPerfLevel);
  if (!EFI_ERROR (Status)) {
    Status = GetClusterPerfLevelFreqMHz (Cluster, CurrentPerfLevel, &CurrentFreqMHz);
    if (EFI_ERROR (Status) && CurrentPerfLevel > 0) {
      Status = GetClusterPerfLevelFreqMHz (Cluster, CurrentPerfLevel - 1, &CurrentFreqMHz);
    }
  }

  if (EFI_ERROR (Status) || CurrentFreqMHz == 0) {
    CurrentFreqMHz = MaxFreqMHz;
  }

  // Pass Cluster Speeds
  *MaxSpeed     = MaxFreqMHz;
  *CurrentSpeed = CurrentFreqMHz;

  return EFI_SUCCESS;
}

EFI_STATUS
GetChipIdString (OUT CHAR8 *IdString)
{
  // Verify Protocol Presense
  if (mChipInfoProtocol == NULL) {
    return EFI_NOT_READY;
  }

  // Get Chip ID String
  return mChipInfoProtocol->GetChipIdString (mChipInfoProtocol, IdString, 16);
}

MEMORY_DEVICE_TYPE
ConvertMemoryType (IN UINT8 RawMemoryType)
{
  // Set SMBIOS Memory Types
  STATIC CONST SMBIOS_MEMORY_TYPE SmBiosMemoryTypes[] = {
    {DDR_TYPE_PCDDR2,  MemoryTypeDdr2},
    {DDR_TYPE_PCDDR3,  MemoryTypeDdr3},
    {DDR_TYPE_LPDDR1,  MemoryTypeLpddr},
    {DDR_TYPE_LPDDR2,  MemoryTypeLpddr2},
    {DDR_TYPE_LPDDR3,  MemoryTypeLpddr3},
    {DDR_TYPE_LPDDR4,  MemoryTypeLpddr4},
    {DDR_TYPE_LPDDR4X, MemoryTypeLpddr4},
    {DDR_TYPE_LPDDR5,  MemoryTypeLpddr5},
    {DDR_TYPE_LPDDR5X, MemoryTypeLpddr5}
  };

  // Convert Memory Type
  for (UINT8 i = 0; i < ARRAY_SIZE (SmBiosMemoryTypes); i++) {
    // Compare Memory Types
    if (RawMemoryType == SmBiosMemoryTypes[i].RawType) {
      return SmBiosMemoryTypes[i].Type;
    }
  }

  return MemoryTypeUnknown;
}

EFI_STATUS
GetDdrDetails (
  OUT UINT8              *ManufacturerId,
  OUT MEMORY_DEVICE_TYPE *MemoryType)
{
  EFI_STATUS        Status;
  ddr_details_entry DdrInfos;

  // Verify Protocol Presense
  if (mDdrInfoProtocol == NULL) {
    return EFI_NOT_READY;
  }

  // Get DDR Details
  Status = mDdrInfoProtocol->GetDDRDetails (mDdrInfoProtocol, &DdrInfos);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Pass DDR Details
  *ManufacturerId = DdrInfos.manufacturer_id;
  *MemoryType     = ConvertMemoryType (DdrInfos.device_type);

  return EFI_SUCCESS;
}

EFI_STATUS
GetDdrSpeeds (
  OUT UINT32 *MaxSpeed,
  OUT UINT32 *CurrentSpeed)
{
  EFI_STATUS          Status;
  ddr_freq_plan_entry DdrFreqTable;

  // Verify Protocol Presense
  if (mDdrInfoProtocol == NULL) {
    return EFI_NOT_READY;
  }

  // Verify Protocol Revision
  if (mDdrInfoProtocol->Revision < 0x30000) {
    return EFI_UNSUPPORTED;
  }

  // Verify Faulty Protocol Flag
  if (FixedPcdGetBool (PcdFaultyDdrProtocol)) {
    return EFI_ABORTED;
  }

  // Get Current DDR Freq
  Status = mDdrInfoProtocol->GetDDRFreq (mDdrInfoProtocol, CurrentSpeed);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Get DDR Freq Table
  Status = mDdrInfoProtocol->GetDDRFreqTable (mDdrInfoProtocol, &DdrFreqTable);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Go thru each DDR Freq
  for (UINT8 i = 0; i < DdrFreqTable.num_ddr_freqs; i++) {
    // Verify DDR Freq
    if (!DdrFreqTable.ddr_freq[i].freq_khz || !DdrFreqTable.ddr_freq[i].enable) {
      continue;
    }

    // Save Max DDR Freq
    if (i == DdrFreqTable.num_ddr_freqs - 1) {
      *MaxSpeed = DdrFreqTable.ddr_freq[i].freq_khz / 1000;
    }
  }

  return EFI_SUCCESS;
}

CHAR8*
ConvertManufacturerId (IN UINT8 Id)
{
  // Set Manufacturer Names
  STATIC CONST SMBIOS_MANUFACTURER_NAME SmBiosManufacturerNames[] =  {
    {1,   "Samsung"},
    {6,   "Hynix"},
    {255, "Micron"}
  };

  // Go thru each Manufacturer Name
  for (UINT8 i = 0; i < ARRAY_SIZE (SmBiosManufacturerNames); i++) {
    // Compare IDs
    if (Id == SmBiosManufacturerNames[i].Id) {
      // Return Manufacturer Name
      return SmBiosManufacturerNames[i].Name;
    }
  }

  // Return Dummy Name
  return "Not Specified";
}

VOID
InitializeUtilities ()
{
  EFI_STATUS Status;

  // Locate Clock Protocol
  Status = gBS->LocateProtocol (&gEfiClockProtocolGuid, NULL, (VOID *)&mClockProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate Clock Protocol! Status = %r\n", Status));
  }

  // Locate DDR Info Protocol
  Status = gBS->LocateProtocol (&gEfiDDRGetInfoProtocolGuid, NULL, (VOID *)&mDdrInfoProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate DDR Info Protocol! Status = %r\n", Status));
  }

  // Locate Chip Info Protocol
  Status = gBS->LocateProtocol (&gEfiChipInfoProtocolGuid, NULL, (VOID *)&mChipInfoProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((EFI_D_ERROR, "Failed to Locate Chip Info Protocol! Status = %r\n", Status));
  }
}
