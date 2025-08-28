#ifndef __MTK_MSDC_H__
#define __MTK_MSDC_H__

#define MAX_MSDC_HOSTS 1

typedef struct {
  UINT32 MsdcMmioReg;
  UINT32 TopMmioReg;
} MSDC_HOST_INFO;

typedef struct {
  MSDC_HOST_INFO HostInfo[MAX_MSDC_HOSTS];
  UINT32 MsdcPadTuneReg;
  BOOLEAN UseTop;
} MSDC_PLATFORM_INFO;

#endif /* __MTK_MSDC_H__ */