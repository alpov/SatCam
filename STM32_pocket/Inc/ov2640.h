#ifndef __OV2640_H__
#define __OV2640_H__

#include "eeprom.h"

#define SLAVE_ADDR          0x60    // I2C address
#define SCCB_TIMEOUT        500     // camera I2C timeout [ms]
#define SENSOR_TIMEOUT      800     // camera DCMI timeout [ms]
#define SENSOR_INIT_RETRY   3       // init retry count

#define AWB_AUTO            0
#define AWB_SUNNY           1
#define AWB_CLOUDY          2
#define AWB_OFFICE          3
#define AWB_HOME            4

#define BANK_SEL_DSP        0x00
#define BANK_SEL_SENSOR     0x01

extern bool ov2640_enable(bool en);
extern bool ov2640_enable_safe(bool en);
extern uint32_t ov2640_snapshot(uint8_t *buffer, uint32_t length);
extern void ov2640_set_awb(uint8_t mode);
extern uint16_t ov2640_get_current_agc(void);
extern uint16_t ov2640_get_current_aec(void);
extern void ov2640_set_register(uint8_t bank, uint8_t reg, uint8_t value);
extern uint8_t ov2640_get_register(uint8_t bank, uint8_t reg);
extern bool ov2640_hilevel_init(CONFIG_CAMERA *cam) __attribute__ ((warn_unused_result));

#endif /* __OV2640_H__ */
