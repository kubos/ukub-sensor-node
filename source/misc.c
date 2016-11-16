#include "misc.h"
#include <kubos-hal/gpio.h>
#include <FreeRTOS.h>
#include <task.h>

inline void blink(int pin) {
    k_gpio_write(pin, 1);
    vTaskDelay(1);
    k_gpio_write(pin, 0);
}
