/*
 * KubOS RT
 * Copyright (C) 2016 Kubos Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "queue.h"

#include "kubos-hal/gpio.h"
#include "kubos-hal/uart.h"
#include "kubos-hal/i2c.h"
#include "kubos-core/modules/klog.h"

#include "kubos-core/modules/sensors/htu21d.h"

#include <csp/csp.h>

static inline void blink(int pin) {
    #define BLINK_MS 100
    k_gpio_write(pin, 1);
    vTaskDelay(50);
    k_gpio_write(pin, 0);
}

void task_sensors(void *p)
{
    float temp = 0;
    float hum = 0;
    uint32_t time_ms;
    htu21d_setup();
    htu21d_reset();
    static char msg[100];

    while(1)
    {
        time_ms = csp_get_ms();
        temp = htu21d_read_temperature();
        hum = htu21d_read_humidity();
        sprintf(msg, "%d||%f||%f\r\n", time_ms, temp, hum);
        k_uart_write(K_UART_CONSOLE, msg, strlen(msg));
        blink(K_LED_RED);
        vTaskDelay(1000);
    }
    while (1)
    {
        vTaskDelay(100);
    }
}

int main(void)
{
    k_uart_console_init();

    k_gpio_init(K_LED_GREEN, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_ORANGE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_RED, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_BLUE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    
    xTaskCreate(task_sensors, "sensors", configMINIMAL_STACK_SIZE * 4, NULL, 2, NULL);

    vTaskStartScheduler();

    return 0;
}
