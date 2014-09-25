#ifndef	_SYS_LOG_H
#define _SYS_LOG_H

/*
 * \brief		calculate file size.
 * \param		fp as file pointer.
 * \return	size of file.
 */
int32_t fsize(FILE *fp);

/**
 * \brief		write log on sdcard.
 * \param		type as {PC, SERVER, SMS, DEVICE...}.
 * \param		logs as string which written.
 * \param		lsize as length of logs.
 * \return	number of bytes which written.
 **/
int16_t sys_log_fwrite(uint8_t type, int8_t *logs, int16_t lsize);

/*
 * \brief		read newest log in file.
 * \param		logs as output buffer.
 * \param		lsize as number bytes for read.
 * \return	number bytes read.
 */
int16_t sys_log_fread(int8_t *logs, int16_t lsize);

#endif //_SYS_LOG_H
