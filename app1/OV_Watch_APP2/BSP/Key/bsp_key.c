#include "bsp_key.h"

#include "main.h"

bool Key1_IsPressed(void)
{
    return HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET;
}

bool WakeKey_IsPressed(void)
{
    return HAL_GPIO_ReadPin(WAKE_GPIO_Port, WAKE_Pin) == GPIO_PIN_SET;
}
