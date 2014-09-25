#include "rtl.h"
#include "stm32f10x.h"
#include <stdio.h>
#include "modules.h"
#include "printer.h"

/* Private macros -------------------- */
#define POS_PrCmd(chr)				USART_PrChar(PRINTER_PORT, (chr))

#define POS_LEFT_JUSTIFICATION		0x00
#define POS_CENTER_JUSTIFICATION 	0x01
#define POS_RIGHT_JUSTIFICATIOn		0x02

/* Private variables ------------------ */
static uint8_t PosIdx = 0;


/**
 * @fn		void vPosPrinterInit(void)
 * @brief	Init POS Printer
 * @param	none
 */
void vPosPrinterInit(void){
	/* Initialize printer: Clears the data in the print buffer and resets
	the printer mode to the mode that was in effect when the power was turned on. */
	// Command: ESC @	(0x1B 0x40)
	POS_PrCmd(0x1B);
	POS_PrCmd(0x40);

	/* Select character code table: 0 - CP437 [USA, Standard Europe] */
	// Command: ESC t n (0x1B 0x74 t)
	POS_PrCmd(0x1B);
	POS_PrCmd(0x74);
	POS_PrCmd(0x00);						// CP437 [USA, Standard Europe]

	/* Select print mode(s) */
	// Command: ESC ! n (0x1B 0x21 n)
	POS_PrCmd(0x1B);
	POS_PrCmd(0x21);
	POS_PrCmd(0x00);		// Character Font B (9x24), other A(12x24)

	/* Select justification: Left (0,48), center (1,49), Right (2, 50) */
	// Command: ESC a n (0x1B 0x61 n)
	POS_PrCmd(0x1B);
	POS_PrCmd(0x61);
	POS_PrCmd(POS_LEFT_JUSTIFICATION);
}

/**
 * @fn		void vPosPrinterFinish(void)
 * @brief	Feed paper and cut paper
 * @param	none
 */
void vPosPrinterFinish(void){
	/* Print and feed paper */
	// Command: ESC J n (0x1B 0x4A n)
	POS_PrCmd(0x1B);
	POS_PrCmd(0x4A);
	POS_PrCmd(0xF0);

	/* Cut paper: Select cut mode and cut paper */
	// Command: ESC V m (0x1D	0x56 m)
	POS_PrCmd(0x1D);
	POS_PrCmd(0x56);
	POS_PrCmd(0x00);
}

/* ----------------------------------------- */
/**
 * @fn		void vPos_PrCRLF(void)
 * @brief	Print CRLF and clear pos
 */
void vPos_PrCRLF(void){
	POS_PrCmd('\r');
	POS_PrCmd('\n');
	// Reset pos
	PosIdx = 0;
}

/**
 * @fn		void vPosLineWrapper(void)
 * @brief	Wrap line
 */
void vPosLineWrapper(void){
	if(++PosIdx == POS_PRINTER_MAX_LINE)
		vPos_PrCRLF();
}

/* ----------------------------------------- */
/**
 * @fn		void vPos_PrChar(uint8_t chr)
 * @brief	Put char into POS Printer
 * @param	chr Character to print
 */
void vPos_PrChar(uint8_t chr){
	POS_PrCmd(chr);
	// Wrapper and index
	if( (chr != '\r')  && (chr != '\n'))
		vPosLineWrapper();
	else PosIdx = 0;
}

/**
 * @fn		void vPos_PrCStr(uint8_t *data)
 * @brief	Print constant string to POS Printer
 * @param	data	Pointer to data
 */
void vPos_PrCStr(uint8_t *str){
	while(*str)
		vPos_PrChar(*str++);
}
/**
 *
 */
void vPos_PrStr(uint8_t *str, uint16_t len){
	while(len--)
		vPos_PrChar(*str++);
}

/**
 * @fn		void vPosPrinterTest(void)
 * @brief	Print test
 */
void vPosPrinterTest(void){
uint8_t idx;
	for(idx = 0; idx < 9; idx++){
		vPos_PrCStr("Line ");
		vPos_PrChar(idx + '0');
	};
}
