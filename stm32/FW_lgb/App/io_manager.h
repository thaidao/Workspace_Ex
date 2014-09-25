
#ifndef	_IO_MANAGER_
#define	_IO_MANAGER_

typedef struct _io_message_t{	
	uint32_t time;
	
	float x;
	float y;	
	
	uint16_t i0_count;
	uint16_t i1_count;
	uint16_t i2_count;
	
	uint8_t io_state;
	uint8_t crc8;
}io_message_t;

void io_manager_init(io_message_t *ioMessage);
uint8_t io_read_on_port(void);
int8_t io_get_bit(uint8_t io, uint8_t bit);
int8_t io_any_changed(uint8_t io);
int8_t io_bit_changed(uint8_t io, uint8_t bit);
int8_t io_save_to_sdcard(io_message_t *msg, int16_t size);
int8_t io_read_from_sdcard(uint8_t* buf, int16_t *size);
int8_t io_release_in_sdcard(void);
void io_manager(io_message_t *ioMessage);
void io_wipe_in_sdcard(void);
void io_report_clear(io_message_t *ioMessage);

#endif // _IO_MANAGER_
