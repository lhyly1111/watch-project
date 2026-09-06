#ifndef OV_WATCH_APP2_BSP_BATTERY_H
#define OV_WATCH_APP2_BSP_BATTERY_H

#include <stdint.h>

uint16_t Battery_ReadRaw(void);
float Battery_ReadVoltage(void);
float Battery_ReadVoltageAverage(void);

#endif /* OV_WATCH_APP2_BSP_BATTERY_H */
