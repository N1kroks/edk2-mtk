#ifndef __MTK_MSDC_H__
#define __MTK_MSDC_H__

typedef struct {
  UINT32 MsdcMmioReg;
  UINT32 TopMmioReg;
  UINT32 MsdcPadTuneReg;
  BOOLEAN UseTop;
} MSDC_PLATFORM_INFO;

#endif /* __MTK_MSDC_H__ */