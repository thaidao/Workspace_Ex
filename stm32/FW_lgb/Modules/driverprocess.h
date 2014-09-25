/*
 * driverprocess.h
 *
 *  Created on: Mar 12, 2012
 *      Author: Van Ha
 */

#ifndef DRIVERPROCESS_H_
#define DRIVERPROCESS_H_

/* call in task 1s */
void vUpdateDriverData(void);
void vCheckChangeKey(void);
void updateDriverKey(uint8_t *newDrv_id);

/* api */
void vInitDriver(void);
void vResetDriverData(void);
void vResetDataInDay(void);

/*
 * \brief 	uint8_t getDriverPlugIn(void)
 */
uint8_t getDriverPlugIn(void);

#endif /* DRIVERPROCESS_H_ */
