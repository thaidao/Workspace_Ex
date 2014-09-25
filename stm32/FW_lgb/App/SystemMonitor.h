

#ifndef _SYSTEM_MONITOR_H
#define _SYSTEM_MONITOR_H


#define FLAG_HSIRDY			31
#define FLAG_HSERDY			30
#define FLAG_PLLRDY			29
#define FLAG_LSERDY			28
#define FLAG_LSIRDY			27
#define FLAG_PINRST			26
#define FLAG_PORRST			25
#define FLAG_SFTRST			24
#define FLAG_IWDGRST		23
#define FLAG_WWDGRST		22
#define FLAG_LPWRRST		21


typedef enum
{
	RESULT_OK = 0,
	OPEN_FILE_ERROR = -1,
	DATA_SIZE_ERROR = -2,
	DATA_CHECKSUM_FAIL = -3

}ResultType_e;

typedef struct
{
	uint32_t reset_flag;
	uint16_t reset_count;

	uint8_t lsiCnt;
	uint8_t pinCnt;
	uint8_t porCnt;
	uint8_t sftCnt;
	uint8_t iwdCnt;
	uint8_t wwdCnt;
	uint8_t lpwCnt;
	uint8_t othCnt;
}SystemData_t;

/*
 * \brief 	SystemReport(void)
 * 			Read reset source and counter reset number.
 * 	\param	none.
 * 	\return none
 */
void SystemReport(void);

/*
 * \brief	SystemReportClear(void)
 * 			Clear reset count,
 * 	\param	none.
 * 	\return none.
 */
void SystemReportClear(void);


/*
 * \brief	ResultType_e SystemGetInfoInFile(SystemData_t *sysInfo)
 * 			Read file system.log. Get information as reset source, reset count, ...
 * \param	a point type SystemData_t which store information
 * \return	result type. see file .h for more details.
 */
ResultType_e SystemGetInfoInFile(SystemData_t *sysInfo);

/*
 * \brief	ResultType_e SystemSaveInfoInFile(SystemData_t *sysInfo)
 * 			Save file system.log. Get information as reset source, reset count, ...
 * \param	a pointer type SystemData_t point to data
 * \return	result type. see file .h for more details.
 */
ResultType_e SystemSaveInfoInFile(SystemData_t *sysInfo);

ResultType_e SystemMakeFile(void);

#endif
