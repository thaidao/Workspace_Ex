/**
 * file: adminSecurity.c
 *
 **/
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <rtl.h>

#include "stm32f10x.h"
#include "platform_config.h"

#include "loadconfig.h"


const char *tblContent[] = 
{
	"SCFG,SET",		// set config
	"SOS_RESET",  // reset device
	"BRST",				// reset buffer logs
	"CRST",				// reset camera logs
	"RLOG",				// read daily logs.
	"DOTA",				// update firmware.
};
// add to tblContent then increase tblNumber.
#define tblNUMBER		6

/*
 * \brief		BOOL user_validate(int8_t* user)
 *					validation phone number. Phone number must numberic or plus char.
 * \param		user as phone number.
 * \return	true for all char is numberic or plus. otherwise fale.
 */
BOOL user_validate(int8_t* user)
{
	int userLen = 0;
	// validate numberic
	while(*user != NULL){
		if(((*user > '9') || (*user < '0')) && (*user != '+'))
			return __FALSE;
		user++;
		userLen ++;
		if(userLen >16)
			return __FALSE;
	}
	// validate length
	if(userLen < 9)	
		return __FALSE;
	
	return __TRUE;
}

/*
 * \brief		BOOL content_validate(int8_t* pass, int8_t* content)
 *					validation content. Content must include password.
 * \param		pass is store in flash.
 * \param		content as validate.
 * \return	true if content include password. otherwise false.
 */
BOOL content_validate(int8_t* pass, int8_t* content)
{
// 	int i;
// 	
// 	for(i=0;i<tblNUMBER;i++){
// 		if(strstr((const char*)content, tblContent[i])!=NULL)
// 		{
			if(strncmp((const char*)content, (const char*)pass, strlen((char*)pass))==0){
				return __TRUE;
			}
// 		}
// 	}
	
	return __FALSE;
}

/*
 * \brief		void user_encode(int8_t* user, int8_t* userEnc)
 *					encode data.
 * \param		user as input for encoding.
 * \param		userEnc as output for encoded.
 * \return	none.
 */
void user_encode(int8_t* user, int8_t* userEnc)
{
	// select a encoding method.
	strcpy((char*)userEnc, (const char*)user);
}

/*
 * \brief		void user_decode(int8_t* userEnc, int8_t* user)
 *					encode data.
 * \param		userEnc as input for decoding.
 * \param		user as output for decoded.
 * \return	none.
 */
void user_decode(int8_t* userEnc, int8_t* user)
{
	// select a encoding method.
	strcpy((char*)user, (const char*)userEnc);
}

/*
 * \brief		BOOL user_change(int8_t* old_user, int8_t *new_user)
 *					change user.
 * \param		old_user as string.
 * \param		new_user as string.
 * \return	true - success. otherwise false.
 */
BOOL user_change(int8_t* old_user, int8_t *new_user)
{
	int8_t nUser[17];
	if(strlen((const char*)new_user) > 16)
		return __FALSE;
	
	// decode user
	user_decode(new_user, nUser);
	// validate.
	if(!user_validate(old_user))
		return __FALSE;
	
	if(!user_validate(nUser))
		return __FALSE;
	// if dont supper user, reject user.
	if(strcmp((const char*)old_user, (const char*)Config.admin.user)!=0)
		return __FALSE;
	// copy new user.
	strcpy((char*)Config.admin.user, (const char*)nUser);
	// write new config to flash.
	config_apply(&Config);
	
	return __TRUE;
}

/*
 * \brief		BOOL pass_change(int8_t* user, int8_t *new_pass)
 *					change password. check user must admin.
 * \param		user as string.
 * \param		new_pass as string.
 * \return	true - success. otherwise false.
 */
BOOL pass_change(int8_t* user, int8_t *new_pass)
{
	int8_t nPass[7];
	if(strlen((const char*)new_pass) > 6)
		return __FALSE;
	
	// decode pass.
	user_decode(new_pass, nPass);
	
	if(!user_validate(user))
		return __FALSE;
	
	if(strcmp((const char*)user, (const char*)Config.admin.user)!=0)
		return __FALSE;
	
	// copy new pass.
	strcpy((char*)Config.admin.pass, (const char*)nPass);
	// write new config to flash.
	config_apply(&Config);
	
	return __TRUE;
}


// void content_modify(int8_t* content)
// {
// 	if(content_validate("EPOSI", content)){
// 		strcpy((char*)content, (const char*)content + strlen("EPOSI") + 1);
// 	}
// }
