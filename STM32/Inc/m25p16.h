#ifndef _M25P16_H_
#define _M25P16_H_

// M25P16 flash identification
#define M25P16_ID_MANUFACTURER  0x20
#define M25P16_ID_DEVICE_HI     0x20
#define M25P16_ID_DEVICE_LO     0x15

#define M25P16_TIMEOUT_PAGE     6       // timeout [ms] for page write
#define M25P16_TIMEOUT_SECTOR   3000    // timeout [ms] for sector erase
#define M25P16_TIMEOUT_BULK     20000   // timeout [ms] for bulk erase

#define M25P16_INIT_RETRY       3       // retry count for flash init

#define ADDR_JPEGIMAGE(__id)    (((__id) * 2 + 0) << 16)    // JPEG pages - odd - 0, 2, 4, ...
#define ADDR_THUMBNAIL(__id)    (((__id) * 2 + 1) << 16)    // thumbnail pages - even - 1, 2, 3, ... from 0x0000
#define ADDR_FLASHINFO(__id)    ((((__id) * 2 + 1) << 16) + 0x00004000) // flash info after thumbnails ... from 0x4000

extern bool flash_init(void);
extern void flash_read(uint32_t addr, uint8_t *buffer, uint16_t length);
extern bool flash_program_page(uint32_t addr, uint8_t *buffer);
extern bool flash_program(uint32_t addr, uint8_t *buffer, uint16_t length);
extern bool flash_erase_sector(uint32_t addr);
extern bool flash_erase_bulk(void);

#endif /* _M25P16_H_ */
