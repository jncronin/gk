source Firmware/gdbtrace.init
monitor SWO EnableTarget 480000000 2000000 0xFF 0
enableSTM32SWO 7
prepareSWO 480000000 2000000 0 0
 
dwtSamplePC 1
dwtSyncTap 3
dwtPostTap 1
dwtPostInit 1
dwtPostReset 15
dwtCycEna 1
 
ITMId 1
ITMGTSFreq 3
ITMTSPrescale 3
ITMTXEna 1
ITMSYNCEna 1
ITMEna 1
 
ITMTER 0 0xFFFFFFFF
ITMTPR 0xFFFFFFFF
