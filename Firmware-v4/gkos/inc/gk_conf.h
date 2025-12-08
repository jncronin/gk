#ifndef GK_CONF_H
#define GK_CONF_H

#define GK_NUM_CORES                1

#define GK_ENABLE_NETWORK           0
#define GK_ENABLE_WIFI              0
#define GK_ENABLE_USB               1
#define GK_ENABLE_USB_MASS_STORAGE  1
#define GK_ENABLE_TEST_THREADS      0
#define GK_ENABLE_TOUCH             1
#define GK_ENABLE_TILT              1
#define GK_ENABLE_USB_CDC           0
#define GK_CHECK_USER_ADDRESSES     1
#define GK_USE_IRQ_PRIORITIES       0
#define GK_USE_CACHE                1
#define GK_USE_LSE_RTC              1
#define GK_EXT_READONLY             0
#define GK_EXT_USE_JOURNAL          0
#define GK_SD_USE_HS_MODE           1
#define GK_GPU_SHOW_FPS             0
#define GK_TICKLESS                 0
#define GK_DYNAMIC_SYSTICK          1
#define GK_MAXTIMESLICE_US          200000
#define GK_MEMBLK_STATS             1
#define GK_ENABLE_PROFILE           0
#define GK_AUDIO_LATENCY_LIMIT_MS   50
#define GK_PIPESIZE                 65536
#define GK_SCREEN_WIDTH             800
#define GK_SCREEN_HEIGHT            480

#define GK_TLBI_AFTER_TTBR_CHANGE   1

#define GK_DEBUG_BLOCKING           1

#define GK_LOG_PERSISTENT           0
#define GK_LOG_RTT                  1
#define GK_LOG_USB                  0
#define GK_LOG_FILE                 1

#define GK_LOG_SIZE                 (64*1024)

#define GK_NUM_EVENTS_PER_PROCESS   32
#define GK_PRIORITY_IDLE    0
#define GK_PRIORITY_LOW     1
#define GK_PRIORITY_NORMAL  2
#define GK_PRIORITY_GAME    GK_PRIORITY_NORMAL
#define GK_PRIORITY_APP     GK_PRIORITY_NORMAL
#define GK_PRIORITY_HIGH    3
#define GK_PRIORITY_VHIGH   4
#define GK_PRIORITY_VERYHIGH    GK_PRIORITY_VHIGH

#define GK_NPRIORITIES      (GK_PRIORITY_VHIGH + 1)

#define GK_MAX_WINDOW_TITLE 32

#define GK_REBOOTFLAG_RAWSD         1
#define GK_REBOOTFLAG_AUDIOTEST     2
#define GK_REBOOTFLAG_VIDEOTEST     4

#endif
