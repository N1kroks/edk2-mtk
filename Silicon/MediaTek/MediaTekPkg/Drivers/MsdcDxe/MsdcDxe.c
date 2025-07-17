#include "MsdcDxe.h"
#include <Protocol/BlockIo.h>
#include <Protocol/DevicePath.h>

#define BLOCK_SIZE      512

// TODO: Add multiple host support
MSDC_HOST_DATA     *HostData;
SD_INFO            *SdInfo;

MSDC_PLATFORM *MsdcPlatform;

EFI_BLOCK_IO_MEDIA gSDMMCMedia = {
  SIGNATURE_32('m','s','d','c'),            // MediaId
  FALSE,                                    // RemovableMedia
  TRUE,                                     // MediaPresent
  FALSE,                                    // LogicalPartition
  FALSE,                                    // ReadOnly
  FALSE,                                    // WriteCaching
  512,                                      // BlockSize
  4,                                        // IoAlign
  0,                                        // Pad
  0                                         // LastBlock
};


typedef struct {
  VENDOR_DEVICE_PATH  Mmc;
  EFI_DEVICE_PATH     End;
} MSDC_DEVICE_PATH;

MSDC_DEVICE_PATH gMSDCDevicePath = {
  {  // Mmc (VENDOR_DEVICE_PATH)
    {  // Header
      HARDWARE_DEVICE_PATH,      // Type
      HW_VENDOR_DP,              // SubType
      {                          // Length (UINT8 array style)
        (UINT8)(sizeof(VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof(VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    // Vendor GUID
    { 0xb615f1f5, 0x5088, 0x43cd, { 0x80, 0x9c, 0xa1, 0x6e, 0x52, 0x48, 0x7d, 0x00 } }
  },
  {  // End (EFI_DEVICE_PATH)
    END_DEVICE_PATH_TYPE,        // Type
    END_ENTIRE_DEVICE_PATH_SUBTYPE,  // SubType
    {                            // Length (UINT8 array style)
      sizeof(EFI_DEVICE_PATH_PROTOCOL),
      0
    }
  }
};

VOID MsdcReset ()
{
  UINT32 Reg;
  MsdcSetBits (MSDC_CFG, MSDC_CFG_RST);
  do
  {
    MsdcRead (MSDC_CFG, &Reg);
    MicroSecondDelay (100);
  } while (!(Reg & MSDC_CFG_RST));
}

VOID MsdcClearFifo ()
{
  
  UINT32 Reg;
  MsdcSetBits (MSDC_FIFOCS, MSDC_FIFOCS_CLR);
  do
  {
    MsdcRead (MSDC_FIFOCS, &Reg);
    MicroSecondDelay (100);
  } while (!(Reg & MSDC_FIFOCS_CLR));
}

VOID MsdcClearInterrupts ()
{
  UINT32 Reg;
  MsdcRead (MSDC_INT, &Reg);
  MsdcWrite (MSDC_INT, Reg);
}

VOID MsdcSetTimeout ()
{
  UINT32 CfgReg, Timeout, ClkNs;

  ClkNs = 1000000000 / HostData->Sclk;
  Timeout = (HostData->TimeoutNs + ClkNs - 1) / ClkNs + HostData->TimeoutClks;
  Timeout = (Timeout + (1 << SCLK_CYCLES_SHIFT) - 1) >> SCLK_CYCLES_SHIFT;
  Timeout = Timeout > 1 ? Timeout - 1 : 0;
  Timeout = Timeout > 255 ? 255 : Timeout;

  MsdcRead (SDC_CFG, &CfgReg);
  // Clear BIT24:BIT31
  CfgReg &= ~(0xff << 24);

  CfgReg |= Timeout << 24;
  MsdcWrite (SDC_CFG, CfgReg);
}

VOID MsdcSetBusWidth (UINT32 Width)
{
  UINT32 CfgReg, BusWidth;

  switch (Width) {
    case 1:
      BusWidth = MSDC_BUS_WIDTH_1;
      break;
    case 4:
      BusWidth = MSDC_BUS_WIDTH_4;
      break;
    case 8:
      BusWidth = MSDC_BUS_WIDTH_8;
      break;
    default:
      BusWidth = MSDC_BUS_WIDTH_1;
      break;
  }
  
  MsdcRead (SDC_CFG, &CfgReg);
  // Clear BIT16:BIT17
  CfgReg &= ~(3 << SDC_CFG_BUS_WIDTH_SHIFT);

  CfgReg |= BusWidth << SDC_CFG_BUS_WIDTH_SHIFT;
  MsdcWrite (SDC_CFG, CfgReg);
}

VOID MsdcSetMclk (UINT32 Hz)
{
  UINTN SourceClock;
  UINT32 Div, Mode;
  MsdcPlatform->GetSourceClockRate (&SourceClock);
  
  if (Hz >= SourceClock) {
    // ignore divisor
    Div = 0;
    Mode = MSDC_MCLK_NO_DIV;
    HostData->Sclk = SourceClock;
  } else {
    // divisor mode
    Mode = MSDC_MCLK_DIV;
    if (Hz >= (SourceClock >> 1)) {
      Div = 0; /* will divide source clock 1/2 */
      HostData->Sclk = SourceClock >> 1;
    } else {
      Div = (SourceClock + ((Hz << 2) - 1)) / (Hz << 2);
      HostData->Sclk = (SourceClock >> 2) / Div;
    }
  }

  DEBUG ((DEBUG_INFO, "Hz: %d, Mode: %d, Div: %d, Sclk: %d\n", Hz, Mode, Div, HostData->Sclk));

  UINT32 CfgReg;
  MsdcRead (MSDC_CFG, &CfgReg);
  // clear CCKMD (BIT20:21)
  CfgReg &= ~(3 << 20);
  // clear CCKDIV (BIT8:19)
  CfgReg &= ~(0xfff << 8);
  // clear hs400 div
  CfgReg &= ~(MSDC_CFG_HS400CKMD);
  // set values
  CfgReg |= (Mode << 20);
  CfgReg |= (Div << 8);

  // disable source clock before changing mclk
  MsdcClrBits (MSDC_CFG, MSDC_CFG_CCKPD);
  MsdcPlatform->DisableSourceClock ();
  // change settings
  MsdcWrite (MSDC_CFG, CfgReg);
  // now we can enable source clock
  MsdcPlatform->EnableSourceClock ();
  // wait untill clock will be stable
  do
  {
    MsdcRead (MSDC_CFG, &CfgReg);
    MicroSecondDelay (100);
  } while (!(CfgReg & MSDC_CFG_CCKSB));

  // Reenable clock
  MsdcSetBits (MSDC_CFG, MSDC_CFG_CCKPD);

  // Needed because clock has been changed
  MsdcSetTimeout ();
}

VOID MsdcCheckBusy (BOOLEAN *IsBusy)
{
  UINT32 Reg;
  MsdcRead (SDC_STS, &Reg);
  *IsBusy = (Reg & SDC_STS_BUSY);
}

#define MSDC_INT_CMDSTS (MSDC_INT_CMDRDY  | MSDC_INT_CMDTMO  | MSDC_INT_CMDCRCERR | \
                         MSDC_INT_ACMDRDY | MSDC_INT_ACMDTMO | MSDC_INT_ACMDCRCERR)
#define MSDC_INT_DATSTS (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR | MSDC_INT_XFER_COMPL)

EFI_STATUS
MsdcIntTrackError (
  UINT32 IntMask,
  UINT32 IntStatus
  )
{
  // Clear interrupts
  MsdcWrite (MSDC_INT, IntStatus & IntMask);

  if (IntStatus & MSDC_INT_CMDTMO) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: NOO, WE GOT CMDTMO INTERRUPT :(\n"));
    return EFI_TIMEOUT;
  }
  
  if (IntStatus & MSDC_INT_CMDCRCERR) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: NOO, WE GOT CMDCRCERR INTERRUPT :(\n"));
    return EFI_CRC_ERROR;
  }

  if (IntStatus & MSDC_INT_ACMDTMO) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: NOO, WE GOT ACMDTMO INTERRUPT :(\n"));
    return EFI_TIMEOUT;
  }

  if (IntStatus & MSDC_INT_ACMDCRCERR) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: NOO, WE GOT ACMDCRCERR INTERRUPT :(\n"));
    return EFI_CRC_ERROR;
  }

  if (IntStatus & MSDC_INT_DATTMO) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: NOO, WE GOT DATTMO INTERRUPT :(\n"));
    return EFI_TIMEOUT;
  }

  if (IntStatus & MSDC_INT_DATCRCERR) {
    DEBUG ((DEBUG_ERROR, "MsdcDxe: NOO, WE GOT DATCRCERR INTERRUPT :(\n"));
    return EFI_CRC_ERROR;
  }

  return EFI_DEVICE_ERROR;
}

EFI_STATUS
MsdcPollInterrupts (
  IN UINT32 ExpectedInterrupts,
  IN UINT32 SuccessInterrupts
  )
{
  UINT32 IntStatus;

  while (TRUE) {
    MsdcRead (MSDC_INT, &IntStatus);
    if (IntStatus & (ExpectedInterrupts)) {
      break;
    }
  }

  if (IntStatus & SuccessInterrupts) {
    MsdcWrite (MSDC_INT, IntStatus & SuccessInterrupts);
    return EFI_SUCCESS;
  }

  return MsdcIntTrackError (ExpectedInterrupts, IntStatus);
}

VOID
MsdcFifoRxBytes (
  UINT32 *RxBytes
  )
{
  UINT32 FifoCs;

  MsdcRead (MSDC_FIFOCS, &FifoCs);
  // BIT0:BIT7
  *RxBytes = FifoCs & 0xff;
}

VOID
MsdcFifoTxBytes (
  UINT32 *TxBytes
  )
{
  UINT32 FifoCs;

  MsdcRead (MSDC_FIFOCS, &FifoCs);
  // BIT16:BIT23
  *TxBytes = FifoCs & (0xff << 16);
}

VOID
MsdcFifoRead (
  UINT8 *ByteBuffer,
  UINT32 BufferLength
  )
{
  UINT32  RemainSize = BufferLength;

  // Pointer align
  while ((UINTN)ByteBuffer % 4 != 0 && RemainSize > 0) {
    *ByteBuffer = MmioRead8 (PlatformInfo.MsdcMmioReg + MSDC_RXDATA);
    ByteBuffer++;
    RemainSize--;
  }

  while (RemainSize >= 4) {
    *(UINT32 *)ByteBuffer = MmioRead32 (PlatformInfo.MsdcMmioReg + MSDC_RXDATA);
    ByteBuffer += 4;
    RemainSize -= 4;
  }

  while (RemainSize) {
    *ByteBuffer = MmioRead8 (PlatformInfo.MsdcMmioReg + MSDC_RXDATA);
    ByteBuffer++;
    RemainSize--;
  }
}

VOID
MsdcFifoWrite (
  UINT8 *ByteBuffer,
  UINT32 BufferLength
  )
{
  UINT32  RemainSize = BufferLength;

  // Pointer align
  while ((UINTN)ByteBuffer % 4 != 0 && RemainSize > 0) {
    MmioWrite8 (PlatformInfo.MsdcMmioReg + MSDC_TXDATA, *ByteBuffer);
    ByteBuffer++;
    RemainSize--;
  }

  while (RemainSize >= 4) {
    MmioWrite32 (PlatformInfo.MsdcMmioReg + MSDC_TXDATA, *ByteBuffer);
    ByteBuffer += 4;
    RemainSize -= 4;
  }

  while (RemainSize) {
    MmioWrite8 (PlatformInfo.MsdcMmioReg + MSDC_TXDATA, *ByteBuffer);
    ByteBuffer++;
    RemainSize--;
  }
}

EFI_STATUS
MsdcPioRead (
  VOID  *Buffer,
  UINT32 BufferLength
  )
{
  EFI_STATUS Status;
  BOOLEAN IsXferDone;
  UINT32 IntStatus, ChunkSize, RemainSize, RxBytes;
  UINT8 *ByteBuffer = (UINT8 *)Buffer;
  UINT32 TmoClks = 3;

  RemainSize = BufferLength;

  //DEBUG ((DEBUG_INFO, "MsdcDxe: Reading PIO with size %d    \n", BufferLength));

  /*if (BufferLength > 1) {
    TmoClks = BufferLength / 512;
    if (TmoClks > 64) {
      TmoClks = 64;
    }
  }
  HostData->TimeoutClks = TmoClks * (1 << SCLK_CYCLES_SHIFT);
  MsdcSetTimeout ();*/

  MsdcClrBits (MSDC_INTEN, MSDC_INT_DATSTS);

  while (TRUE) {
    MsdcRead (MSDC_INT, &IntStatus);

    if (IntStatus & (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR)) {
      return MsdcIntTrackError (MSDC_INT_DATSTS, IntStatus);
    }


    ChunkSize = RemainSize > MSDC_FIFO_SIZE ? MSDC_FIFO_SIZE : RemainSize;
    MsdcFifoRxBytes (&RxBytes);

    if (RemainSize == 0 && RxBytes) {
      ASSERT (FALSE);
    }
    
    if (RxBytes >= ChunkSize) {
      MsdcFifoRead (ByteBuffer, ChunkSize);
      ByteBuffer += ChunkSize;
      RemainSize -= ChunkSize;
    }

    if (IntStatus & MSDC_INT_XFER_COMPL) {
      MsdcWrite (MSDC_INT, IntStatus & MSDC_INT_XFER_COMPL);

      if (RemainSize) {
        DEBUG ((DEBUG_ERROR, "MsdcDxe: Data not fully read :( \n"));
        return EFI_ABORTED;
      }

      break;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcPioWrite (
  VOID  *Buffer,
  UINT32 BufferLength
  )
{
  EFI_STATUS Status;
  BOOLEAN IsXferDone;
  UINT32 IntStatus, ChunkSize, RemainSize, TxBytes;
  UINT8 *ByteBuffer = (UINT8 *)Buffer;

  RemainSize = BufferLength;

  MsdcClrBits (MSDC_INTEN, MSDC_INT_DATSTS);

  while (TRUE) {
    MsdcRead (MSDC_INT, &IntStatus);

    if (IntStatus & (MSDC_INT_DATTMO | MSDC_INT_DATCRCERR)) {
      return MsdcIntTrackError (MSDC_INT_DATSTS, IntStatus);
    }

    if (IntStatus & MSDC_INT_XFER_COMPL) {
      MsdcWrite (MSDC_INT, IntStatus & MSDC_INT_XFER_COMPL);

      if (RemainSize) {
        DEBUG ((DEBUG_ERROR, "MsdcDxe: Data not fully wrote :( \n"));
        return EFI_ABORTED;
      }

      break;
    }

    ChunkSize = RemainSize > MSDC_FIFO_SIZE ? MSDC_FIFO_SIZE : RemainSize;

    MsdcFifoTxBytes (&TxBytes);
    if (MSDC_FIFO_SIZE - TxBytes >= ChunkSize) {
      MsdcFifoWrite (ByteBuffer, ChunkSize);
      ByteBuffer += ChunkSize;
      RemainSize -= ChunkSize;
    }
  }

  return EFI_SUCCESS;
}

EFI_STATUS
MsdcSendCmd (
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET *Packet
  )
{
  BOOLEAN IsBusy, IsDataTransfer, IsRead;
  EFI_STATUS Status;
  UINT32 RawCmd, RspType, BlkLen;
  EFI_SD_MMC_COMMAND_BLOCK *CommandBlk = Packet->SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK  *SdMmcStatusBlk = Packet->SdMmcStatusBlk;

  RawCmd = CommandBlk->CommandIndex;
  IsDataTransfer = FALSE;
  IsRead = TRUE;

  if (CommandBlk->CommandType != SdMmcCommandTypeBc) {
    switch (CommandBlk->ResponseType) {
      case SdMmcResponseTypeR1:
      case SdMmcResponseTypeR5:
      case SdMmcResponseTypeR6:
      case SdMmcResponseTypeR7:
        RspType = 1;
        break;
      case SdMmcResponseTypeR2:
        RspType = 2;
        break;
      case SdMmcResponseTypeR3:
        RspType = 3;
        break;
      case SdMmcResponseTypeR4:
        RspType = 4;
        break;
      case SdMmcResponseTypeR1b:
        RspType = 7;
        break;
      case SdMmcResponseTypeR5b:
        DEBUG ((DEBUG_INFO, "MsdcDxe: Response type R5b is NOT supported!\n"));
        return EFI_ABORTED;
      default:
        DEBUG ((DEBUG_INFO, "MsdcDxe: What the fuck r u want to get? 0x%x\n", CommandBlk->ResponseType));
        return EFI_ABORTED;
    }
    RawCmd |= RspType << SDC_CMD_RSP_TYPE_SHIFT;
  }

  if (CommandBlk->CommandIndex == SD_READ_SINGLE_BLOCK) {
    // single block transaction
    RawCmd |= SDC_CMD_SINGLE_BLK;
    IsDataTransfer = TRUE;
    
    BlkLen = 1;
  }
  
  if (CommandBlk->CommandIndex == SD_READ_MULTIPLE_BLOCK) {
    // multiple block transaction
    RawCmd |= SDC_CMD_MULTIPLE_BLK;
    IsDataTransfer = TRUE;

    BlkLen = Packet->OutTransferLength / 512;
  }
  
  if (CommandBlk->CommandIndex == SD_WRITE_SINGLE_BLOCK) {
    // write mode
    RawCmd |= SDC_CMD_RW;
    // single block transaction
    RawCmd |= SDC_CMD_SINGLE_BLK;

    IsDataTransfer = TRUE;
    IsRead = FALSE;

    BlkLen = 1;
  }
  
  if (CommandBlk->CommandIndex == SD_WRITE_MULTIPLE_BLOCK) {
    // write mode
    RawCmd |= SDC_CMD_RW;
    // multiple block transaction
    RawCmd |= SDC_CMD_MULTIPLE_BLK;

    IsDataTransfer = TRUE;
    IsRead = FALSE;
  }
  
  if (CommandBlk->CommandIndex == SD_STOP_TRANSMISSION) {
    // stop command
    RawCmd |= SDC_CMD_STOP_CMD;
  }

  if (IsDataTransfer) {
    RawCmd |= BLOCK_SIZE << SDC_CMD_BLK_SIZE_SHIFT;
    MsdcWrite (SDC_BLK_NUM, BlkLen);
  }

  do {
    MsdcCheckBusy (&IsBusy);
    MicroSecondDelay (100);
  } while (IsBusy);

  // Disable generating interrupts, cuz we use polling way
  MsdcClrBits (MSDC_INTEN, MSDC_INT_CMDSTS);
  
  if (IsDataTransfer) {
    //DEBUG ((DEBUG_INFO, "MsdcDxe: Sending CMD with OpCode: %d\n", CommandBlk->CommandIndex));
    //DEBUG ((DEBUG_INFO, "MsdcDxe: CMD Arguments: 0x%x\n", CommandBlk->CommandArgument));
    //DEBUG ((DEBUG_INFO, "MsdcDxe: MSDC Command: 0x%x\n", RawCmd));
  }

  MsdcWrite (SDC_ARG, CommandBlk->CommandArgument);
  MsdcWrite (SDC_CMD, RawCmd);

  if (IsDataTransfer) {
    //DEBUG ((DEBUG_INFO, "MsdcDxe: Command sent!\n"));
  }
  Status = MsdcPollInterrupts (MSDC_INT_CMDSTS, MSDC_INT_CMDRDY);
  if (IsDataTransfer) {
    //DEBUG ((DEBUG_INFO, "MsdcDxe: Command done!\n"));
  }
  if (EFI_ERROR(Status)) {
    return Status;
  }
  
  if (CommandBlk->CommandType != SdMmcCommandTypeBc) {
    if (CommandBlk->ResponseType == SdMmcResponseTypeR2) {
      MsdcRead (SDC_RESP0, &SdMmcStatusBlk->Resp0);
      MsdcRead (SDC_RESP1, &SdMmcStatusBlk->Resp1);
      MsdcRead (SDC_RESP2, &SdMmcStatusBlk->Resp2);
      MsdcRead (SDC_RESP3, &SdMmcStatusBlk->Resp3);
    } else {
      MsdcRead (SDC_RESP0, &SdMmcStatusBlk->Resp0);
    }
  }

  if (IsDataTransfer) {
    if (IsRead) {
      Status = MsdcPioRead (Packet->OutDataBuffer, Packet->OutTransferLength);
      if (EFI_ERROR(Status)) {
        return Status;
      }
    } else {
      Status = MsdcPioWrite (Packet->InDataBuffer, Packet->InTransferLength);
      if (EFI_ERROR(Status)) {
        return Status;
      }
    }
  }

  return Status;
}

VOID MsdcInit ()
{
  MsdcPlatform->EnableClocks ();

  // Configure to SD/MMC mode
  MsdcSetBits (MSDC_CFG, MSDC_CFG_MODE);
  // Clock free running
  MsdcSetBits (MSDC_CFG, MSDC_CFG_CCKPD);
  // Use PIO mode
  MsdcSetBits (MSDC_CFG, MSDC_CFG_PIO);
  // SW Reset
  MsdcReset ();
  // Clear FIFO
  MsdcClearFifo ();
  // Mask all interrupts
  MsdcClearInterrupts ();

  // Enable SDIO mode, otherwise cmd5 won't work
  // This bit disables R4 response CRC check for SDIO card
  MsdcSetBits (SDC_CFG, SDC_CFG_SDIO);
  // Disable detecting SDIO interrupts
  MsdcClrBits (SDC_CFG, SDC_CFG_SDIOIDE);

  if (PlatformInfo.UseTop) {
    MsdcTopWrite (TOP_CTRL, 0);
    MsdcTopWrite (TOP_CMD, 0);
  } else {
    MsdcWrite (PlatformInfo.MsdcPadTuneReg, 0);
  }

  MsdcWrite (MSDC_IOCON, 0);

  // Quirks
  MsdcWrite (MSDC_PATCH_BIT0, 0x403c0046);
  MsdcWrite (MSDC_PATCH_BIT1, 0xffff4089);
  // CKGEN DLY SEL to 1
  MsdcSetBits (MSDC_PATCH_BIT0, BIT10);

  MsdcSetBits (EMMC50_CFG0, EMMC50_CFG0_CRCSTSSEL);

  // stop clk fix (FIXME: not all socs require)
  MsdcSetBits (MSDC_PATCH_BIT1, BIT8 | BIT9);
  MsdcClrBits (SDC_FIFO_CFG, SDC_FIFO_CFG_WRVALIDSEL);
  MsdcClrBits (SDC_FIFO_CFG, SDC_FIFO_CFG_RDVALIDSEL);

  // busy check fix (FIXME: not all socs require)
  MsdcClrBits (MSDC_PATCH_BIT1, MSDC_PB1_BUSYCHECKSEL);

  // async fifo (FIXME: not all socs require)
  MsdcSetBits(MSDC_PATCH_BIT2, BIT2 | BIT3);

  // enchance rx (FIXME: not all socs require)
  if (PlatformInfo.UseTop) {
    MsdcTopSetBits (TOP_CTRL, SDC_RX_ENH_EN);
  } else {
    MsdcSetBits (SDC_ADV_CFG0, SDC_RX_ENHANCE_EN);
  }

  // with async fifo we don't need to tune internal delay
  MsdcClrBits (MSDC_PATCH_BIT2, MSDC_PB2_CFGRESP);
  MsdcSetBits (MSDC_PATCH_BIT2, MSDC_PB2_CFGCRCSTS);

  // data tune fuckery
  if (PlatformInfo.UseTop) {
    MsdcTopSetBits (TOP_CTRL, PAD_DAT_RD_RXDLY_SEL);
    MsdcTopClrBits (TOP_CTRL, DATA_K_VALUE_SEL);
    MsdcTopSetBits (TOP_CMD, PAD_CMD_RD_RXDLY_SEL);
    // todo: fix tuning step
  }

  // set default data timeout
  HostData->TimeoutNs = 100000000;
  HostData->TimeoutClks = 3 * (1 << SCLK_CYCLES_SHIFT);

  // Set default bus width
  MsdcSetBusWidth (1);

  // 
  // NO WAY, WE DID INITIALIZATION
  //
}

EFI_STATUS
EFIAPI
MtkMmcPassThru (
  IN     EFI_SD_MMC_PASS_THRU_PROTOCOL        *This,
  IN     UINT8                                Slot,
  IN OUT EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  *Packet,
  IN     EFI_EVENT                            Event    OPTIONAL
  )
{
  if ((This == NULL) || (Packet == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->SdMmcCmdBlk == NULL) || (Packet->SdMmcStatusBlk == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->OutDataBuffer == NULL) && (Packet->OutTransferLength != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  if ((Packet->InDataBuffer == NULL) && (Packet->InTransferLength != 0)) {
    return EFI_INVALID_PARAMETER;
  }

  return MsdcSendCmd (Packet);
}

EFI_STATUS
SdCardReset ()
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_GO_IDLE_STATE;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeBc;

  Status = MsdcSendCmd (&Packet);

  return Status;
}

EFI_STATUS SdSendIfCond ()
{
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_STATUS                           Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_SEND_IF_COND;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeBcr;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR7;
  SdMmcCmdBlk.CommandArgument = 0x1AA;

  Status = MsdcSendCmd (&Packet);
  if (EFI_ERROR(Status)) {
    return Status;
  }
  
  return Status;
}

EFI_STATUS
SdCardSendOpCond (
  IN     UINT16                         Rca,
  IN     UINT32                         VoltageWindow,
  IN     BOOLEAN                        S18R,
  IN     BOOLEAN                        Xpc,
  IN     BOOLEAN                        Hcs,
  OUT UINT32                            *Ocr
  )
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;
  UINT32                               Switch;
  UINT32                               MaxPower;
  UINT32                               HostCapacity;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_APP_CMD;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcSendCmd (&Packet);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SdMmcCmdBlk.CommandIndex = SD_SEND_OP_COND;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeBcr;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR3;

  Switch       = S18R ? BIT24 : 0;
  MaxPower     = Xpc ? BIT28 : 0;
  HostCapacity = Hcs ? BIT30 : 0;

  SdMmcCmdBlk.CommandArgument = (VoltageWindow & 0xFFFFFF) | Switch | \
                                MaxPower | HostCapacity;

  Status = MsdcSendCmd (&Packet);

  if (!EFI_ERROR (Status)) {
    *Ocr = SdMmcStatusBlk.Resp0;
  }

  return Status;
}

EFI_STATUS
SdCardAllSendCid ()
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_ALL_SEND_CID;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeBcr;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR2;

  Status = MsdcSendCmd (&Packet);

  return Status;
}

EFI_STATUS
SdCardSetRca (
  OUT UINT16                            *Rca
  )
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_SET_RELATIVE_ADDR;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeBcr;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR6;

  Status = MsdcSendCmd (&Packet);
  if (!EFI_ERROR (Status)) {
    *Rca = (UINT16)(SdMmcStatusBlk.Resp0 >> 16);
  }

  return Status;
}

EFI_STATUS
SdCardGetCsd (
  IN     UINT16                         Rca,
  OUT SD_CSD                            *Csd
  )
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_SEND_CSD;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR2;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcSendCmd (&Packet);
  if (!EFI_ERROR (Status)) {
    CopyMem (Csd, &SdMmcStatusBlk, sizeof (SD_CSD));
  }

  return Status;
}

EFI_STATUS
SdCardSelect (
  IN UINT16                         Rca
  )
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_SELECT_DESELECT_CARD;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeAc;
  if (Rca != 0) {
    SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR1b;
  }

  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcSendCmd (&Packet);

  return Status;
}

EFI_STATUS
SdCardSetBusWidth (
  IN UINT16                         Rca,
  IN UINT8                          BusWidth
  )
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;
  UINT8                                Value;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_APP_CMD;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcSendCmd (&Packet);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SdMmcCmdBlk.CommandIndex = SD_SET_BUS_WIDTH;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR1;

  if (BusWidth == 1) {
    Value = 0;
  } else if (BusWidth == 4) {
    Value = 2;
  } else {
    return EFI_INVALID_PARAMETER;
  }

  SdMmcCmdBlk.CommandArgument = Value & 0x3;

  Status = MsdcSendCmd (&Packet);
  return Status;
}

EFI_STATUS
SdCardSendStatus (
  IN     UINT16                         Rca,
  OUT UINT32                            *DevStatus
  )
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_SEND_STATUS;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = (UINT32)Rca << 16;

  Status = MsdcSendCmd (&Packet);
  if (!EFI_ERROR (Status)) {
    *DevStatus = SdMmcStatusBlk.Resp0;
  }

  return Status;
}

EFI_STATUS
SdCardSwitchBusWidth (
  IN UINT16                         Rca,
  IN UINT8                          BusWidth
  )
{
  EFI_STATUS  Status;
  UINT32      DevStatus;

  Status = SdCardSetBusWidth (Rca, BusWidth);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SdCardSwitchBusWidth: Switch to bus width %d fails with %r\n",
      BusWidth,
      Status
      ));
    return Status;
  }

  Status = SdCardSendStatus (Rca, &DevStatus);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SdCardSwitchBusWidth: Send status fails with %r\n",
      Status
      ));
    return Status;
  }

  //
  // Check the switch operation is really successful or not.
  //
  if ((DevStatus >> 16) != 0) {
    DEBUG ((
      DEBUG_ERROR,
      "SdCardSwitchBusWidth: The switch operation fails as DevStatus 0x%08x\n",
      DevStatus
      ));
    return EFI_DEVICE_ERROR;
  }

  MsdcSetBusWidth (BusWidth);

  return Status;
}

EFI_STATUS
SdCardSwitch (
  IN     UINT8                          AccessMode,
  IN     UINT8                          CommandSystem,
  IN     UINT8                          DriveStrength,
  IN     UINT8                          PowerLimit,
  IN     BOOLEAN                        Mode,
  OUT UINT8                             *SwitchResp
  )
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;
  UINT32                               ModeValue;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex = SD_SWITCH_FUNC;
  SdMmcCmdBlk.CommandType  = SdMmcCommandTypeAdtc;
  SdMmcCmdBlk.ResponseType = SdMmcResponseTypeR1;

  ModeValue                   = Mode ? BIT31 : 0;
  SdMmcCmdBlk.CommandArgument = (AccessMode & 0xF) |            \
                                ((PowerLimit & 0xF) << 4) |     \
                                ((DriveStrength & 0xF) << 8) |  \
                                ((DriveStrength & 0xF) << 12) | \
                                ModeValue;

  Packet.InDataBuffer     = SwitchResp;
  Packet.InTransferLength = 64;

  Status = MsdcSendCmd (&Packet);

  return Status;
}

EFI_STATUS
SdCardSetBusMode (
  IN UINT16                         Rca
  )
{
  EFI_STATUS              Status;
  UINT8                   SwitchResp[64];
  UINT8                   AccessMode;
  UINT32                  ClockFreq;
  UINT32                  DevStatus;

  DEBUG ((
    DEBUG_ERROR,
    "MsdcDxe: SdCardSetBusMode: Set bus width\n"
    ));

  Status = SdCardSwitchBusWidth (Rca, 1);

  ClockFreq  = 50*1000*1000;
  AccessMode = 1;

  Status = SdCardSwitch (AccessMode, 0xF, 0xF, 0xF, TRUE, SwitchResp);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = SdCardSendStatus (Rca, &DevStatus);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SdCardSetBusMode: Send status fails with %r\n",
      Status
      ));
    return Status;
  }

  if (DevStatus & BIT7) {
    DEBUG ((
      DEBUG_ERROR,
      "SdCardSetBusMode: Got switch error status"
      ));
    return EFI_DEVICE_ERROR;
  }

  DEBUG ((
    DEBUG_INFO,
    "SdCardSetBusMode: Switch to AccessMode %d ClockFreq %d \n",
    AccessMode,
    ClockFreq
    ));
  
  MsdcSetMclk (ClockFreq);

  return Status;
}

EFI_STATUS
SdCardIdentification ()
{
  EFI_STATUS                     Status;
  UINT32                         Ocr;
  UINT16                         Rca;
  BOOLEAN                        S18r;
  BOOLEAN                        Xpc;
  BOOLEAN                        Hcs;
  SD_CSD                         Csd;

  // Power up card
  MsdcPlatform->PowerControl (TRUE);

  // let's wait some time after msdc power up
  MicroSecondDelay (20000);

  // Set init clock (400kHz)
  MsdcSetMclk (400000);

  //
  // 1. Send Cmd0 to the device
  //
  
  Status = SdCardReset ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "SdCardIdentification: Executing Cmd0 fails with %r\n",
      Status
      ));
    return Status;
  }

  MicroSecondDelay (10000);
  //
  // 2. Send Cmd8 to the device
  //
  Status = SdSendIfCond ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "SdCardIdentification: Executing Cmd8 fails with %r\n",
      Status
      ));
    return Status;
  }

  //
  // 3. Send Acmd41 with voltage window 0 to the device
  //
  Status = SdCardSendOpCond (0, 0, FALSE, FALSE, FALSE, &Ocr);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_INFO,
      "SdCardIdentification: Executing SdCardSendOpCond fails with %r\n",
      Status
      ));
    return EFI_DEVICE_ERROR;
  }

  S18r = FALSE;
  Xpc = TRUE;
  Hcs = TRUE;

  //
  // 4. Repeatly send Acmd41 with supply voltage window to the device.
  //    Note here we only support the cards complied with SD physical
  //    layer simplified spec version 2.0 and version 3.0 and above.
  //
  do {
    Status = SdCardSendOpCond (0, Ocr, S18r, Xpc, Hcs, &Ocr);
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "SdCardIdentification: SdCardSendOpCond fails with %r Ocr %x, S18r %x, Xpc %x\n",
        Status,
        Ocr,
        S18r,
        Xpc
        ));
      return EFI_DEVICE_ERROR;
    }
  } while ((Ocr & BIT31) == 0);

  Status = SdCardAllSendCid ();
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SdCardIdentification: Executing SdCardAllSendCid fails with %r\n",
      Status
      ));
    return Status;
  }

  Status = SdCardSetRca (&Rca);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SdCardIdentification: Executing SdCardSetRca fails with %r\n",
      Status
      ));
    return Status;
  }

  Status = SdCardGetCsd (Rca, &Csd);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SdCardIdentification: Executing SdCardGetCsd fails with %r\n",
      Status
      ));
    return Status;
  }

  SD_CSD2 *Csd2 = (SD_CSD2 *)&Csd;
  gSDMMCMedia.LastBlock = (Csd2->CSizeLow | (Csd.CSizeHigh << 16)) - 1;

  Status = SdCardSelect (Rca);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "SdCardIdentification: Selecting card fails with %r\n",
      Status
      ));
    return Status;
  }

  //
  // Enter Data Tranfer Mode.
  //
  DEBUG ((DEBUG_INFO, "SdCardIdentification: Found a SD device\n"));

  Status = SdCardSetBusMode (Rca);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

EFI_STATUS
EFIAPI
MSDCFlushBlocks (
  IN EFI_BLOCK_IO_PROTOCOL  *This
  )
{
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
MSDCWriteBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  IN VOID                           *Buffer
  )
{
  return EFI_WRITE_PROTECTED;
}

EFI_STATUS
MsdcReadSingleBlock (
  EFI_LBA Lba,
  UINTN BufferSize,
  VOID *Buffer
  )
{
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_READ_SINGLE_BLOCK;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAdtc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = Lba;

  Packet.OutDataBuffer     = Buffer;
  Packet.OutTransferLength = (UINT32)BufferSize;

  return MsdcSendCmd (&Packet);
}

EFI_STATUS
MsdcReadSingleMultipleBlock (
  EFI_LBA Lba,
  UINTN BufferSize,
  VOID *Buffer
  )
{
  EFI_STATUS Status;
  UINTN BlockSize = 512;
  UINTN BlockNum = BufferSize / BlockSize;
  UINTN RemainingSize = BufferSize % BlockSize;
  UINT8 *BufferPtr = Buffer;
  
  for (UINTN i = 0; i < BlockNum; i++) {
    Status = MsdcReadSingleBlock (Lba + i, BlockSize, BufferPtr);
    if (EFI_ERROR(Status)) {
      return Status;
    }
    BufferPtr += BlockSize;
  }
  
  if (RemainingSize > 0) {
    Status = MsdcReadSingleBlock (Lba + BlockNum, RemainingSize, BufferPtr);
    if (EFI_ERROR(Status)) {
      return Status;
    }
  }
  
  return EFI_SUCCESS;
}

EFI_STATUS
MsdcReadMultipleBlock (
  EFI_LBA Lba,
  UINTN BufferSize,
  VOID *Buffer
  )
{
  return MsdcReadSingleMultipleBlock (Lba, BufferSize, Buffer);
  EFI_SD_MMC_COMMAND_BLOCK             SdMmcCmdBlk;
  EFI_SD_MMC_STATUS_BLOCK              SdMmcStatusBlk;
  EFI_SD_MMC_PASS_THRU_COMMAND_PACKET  Packet;
  EFI_STATUS                           Status;

  ZeroMem (&SdMmcCmdBlk, sizeof (SdMmcCmdBlk));
  ZeroMem (&SdMmcStatusBlk, sizeof (SdMmcStatusBlk));
  ZeroMem (&Packet, sizeof (Packet));

  Packet.SdMmcCmdBlk    = &SdMmcCmdBlk;
  Packet.SdMmcStatusBlk = &SdMmcStatusBlk;

  SdMmcCmdBlk.CommandIndex    = SD_READ_MULTIPLE_BLOCK;
  SdMmcCmdBlk.CommandType     = SdMmcCommandTypeAdtc;
  SdMmcCmdBlk.ResponseType    = SdMmcResponseTypeR1;
  SdMmcCmdBlk.CommandArgument = Lba;

  Packet.OutDataBuffer     = Buffer;
  Packet.OutTransferLength = (UINT32)BufferSize;

  return MsdcSendCmd (&Packet);
}

EFI_STATUS
EFIAPI
MSDCReadBlocks (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN UINT32                         MediaId,
  IN EFI_LBA                        Lba,
  IN UINTN                          BufferSize,
  OUT VOID                          *Buffer
  )
{
  UINTN BlockNum = BufferSize / 512;
  //DEBUG ((DEBUG_INFO, "MsdcDxe: Read lba %d blocks %d    \n", Lba, BlockNum));
  if ( BlockNum == 1 ) {
    return MsdcReadSingleBlock (Lba, BufferSize, Buffer);
  } 
  return MsdcReadMultipleBlock (Lba, BufferSize, Buffer);
}

EFI_STATUS
EFIAPI
MSDCReset (
  IN EFI_BLOCK_IO_PROTOCOL          *This,
  IN BOOLEAN                        ExtendedVerification
  )
{
  return EFI_SUCCESS;
}

EFI_BLOCK_IO_PROTOCOL gBlockIo = {
  EFI_BLOCK_IO_INTERFACE_REVISION,   // Revision
  &gSDMMCMedia,                      // *Media
  MSDCReset,                        // Reset
  MSDCReadBlocks,                   // ReadBlocks
  MSDCWriteBlocks,                  // WriteBlocks
  MSDCFlushBlocks                   // FlushBlocks
};

EFI_STATUS EFIAPI MsdcDxeInitialize (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  EFI_STATUS Status;

  Status = gBS->LocateProtocol (&gMediaTekMsdcPlatformProtocolGuid, NULL, (VOID **)&MsdcPlatform);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  // Initialize pointers
  HostData = AllocateZeroPool (sizeof(HostData));
  SdInfo = AllocateZeroPool (sizeof(SdInfo));

  MsdcInit ();
  SdCardIdentification ();

  Status = gBS->InstallMultipleProtocolInterfaces (
                &ImageHandle,
                &gEfiBlockIoProtocolGuid,    &gBlockIo,
                &gEfiDevicePathProtocolGuid, &gMSDCDevicePath,
                NULL
                );

  return Status;
}
