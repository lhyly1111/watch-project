#include "bsp_battery.h"

#include "adc.h"

#define BATTERY_ADC_FULL_SCALE         4095.0f
#define BATTERY_ADC_REFERENCE_VOLTAGE  3.3f
#define BATTERY_DIVIDER_RATIO          2.0f
#define BATTERY_SAMPLE_COUNT           8U
#define BATTERY_ADC_TIMEOUT_MS         5U

static float Battery_ConvertRawToVoltage(uint16_t raw)
{
    return ((float)raw * BATTERY_ADC_REFERENCE_VOLTAGE /
            BATTERY_ADC_FULL_SCALE) * BATTERY_DIVIDER_RATIO;
}

uint16_t Battery_ReadRaw(void)
{
    uint32_t raw;

    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return 0U;
    }

    if (HAL_ADC_PollForConversion(&hadc1, BATTERY_ADC_TIMEOUT_MS) != HAL_OK)
    {
        (void)HAL_ADC_Stop(&hadc1);
        return 0U;
    }

    raw = HAL_ADC_GetValue(&hadc1);
    (void)HAL_ADC_Stop(&hadc1);

    return (uint16_t)raw;
}

float Battery_ReadVoltage(void)
{
    return Battery_ConvertRawToVoltage(Battery_ReadRaw());
}

float Battery_ReadVoltageAverage(void)
{
    uint32_t sum = 0U;

    for (uint32_t sample = 0U; sample < BATTERY_SAMPLE_COUNT; ++sample)
    {
        sum += Battery_ReadRaw();
    }

    return Battery_ConvertRawToVoltage((uint16_t)(sum / BATTERY_SAMPLE_COUNT));
}
