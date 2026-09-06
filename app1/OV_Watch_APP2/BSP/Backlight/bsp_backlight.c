#include "bsp_backlight.h"
#include "tim.h"

void Backlight_Init(void)
{
    if (HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_3) != HAL_OK)
    {
        Error_Handler();
    }

    Backlight_SetPercent(50U);
}

void Backlight_SetPercent(uint8_t percent)
{
    uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim3);

    if (percent > 100U)
    {
        percent = 100U;
    }

    __HAL_TIM_SET_COMPARE(
        &htim3,
        TIM_CHANNEL_3,
        (period * percent) / 100U
    );
}