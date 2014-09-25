/*
 * http.h
 *
 *  Created on: Feb 12, 2012
 *      Author: Van Ha
 */

#ifndef HTTP_H_
#define HTTP_H_

typedef enum HTTP_DWL_E
{
	FW_INIT_GSM_MODULE = 0,
	FW_CONNECT_SERVER,
	FW_REQUEST_FILE,
	FW_WAIT_REQUEST_FILE,
	FW_REQUEST_CHUNK,
	FW_WAIT_CHUNK_DATA,
	FW_PROCESS_CHUNK_DATA,
	FW_FAIL,
	FW_SUCCESS
} FWState_t;


// debug
void vTestHttp(void);

void vCheckUpgrade(void);
BOOL blIsDownloading(void);
void FWUpgradeDemandHandler(S8* info);

#define blUpdateValid			blIsDownloading
#define waitDownloadFinish()	os_evt_wait_and(RTX_DWL_FINISH_FLAG, 0xFFFF)
void vStartDownload(void);
/*
 * update is available
 */
BOOL FWUpgradeIsRunning(void);

#endif /* HTTP_H_ */
