#ifndef _ESC_PRINTER_MODULE_
#define _ESC_PRINTER_MODULE_

/* Printer configuration ----------------------- */
#define PRINTER_PORT			UART5				// Printer port
#define POS_PRINTER_MAX_LINE	24					// Max characters in line

/* Printer API --------------------------------- */
void vPosPrinterInit(void);
void vPosPrinterFinish(void);

void vPos_PrChar(uint8_t chr);
void vPos_PrCStr(uint8_t *str);
void vPos_PrStr(uint8_t *str, uint16_t len);
void vPos_PrCRLF(void);

void vPosPrinterTest(void);

#endif
