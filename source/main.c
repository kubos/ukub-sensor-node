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
#include "kubos-core/modules/sensors/bno055.h"

#include <csp/csp.h>

static inline void blink(int pin) {
    #define BLINK_MS 100
    k_gpio_write(pin, 1);
    vTaskDelay(1);
    k_gpio_write(pin, 0);
}

void task_sensors(void *p)
{
    float temp = 0;
    float hum = 0;
    double pos_vector = 0;
    // double accel_vector = 0;
    // double magn_vector = 0;
    double gyro_vector = 0;
    // double eul_vector = 0;
    // double linear_vector = 0;
    // double gravity_vector = 0;
    uint32_t time_ms;
    static char msg[200];

    static KI2CStatus bno_stat;

    htu21d_setup();
    htu21d_reset();
    bno_stat = bno055_init(K_I2C1, OPERATION_MODE_NDOF);
    // get_position(&pos_vector);
    while(1)
    {
        blink(K_LED_ORANGE);
        time_ms = csp_get_ms();
        blink(K_LED_ORANGE);
        temp = htu21d_read_temperature();
        blink(K_LED_ORANGE);
        hum = htu21d_read_humidity();
        if (bno_stat != I2C_OK)
        {
            blink(K_LED_RED);
            blink(K_LED_RED);
            bno_stat = bno055_init(K_I2C1, OPERATION_MODE_NDOF);
        }
        else
        {

            blink(K_LED_ORANGE);
            get_position(&pos_vector);
            // blink(K_LED_ORANGE);
            // get_data_vector(VECTOR_ACCELEROMETER, &accel_vector);
            // blink(K_LED_ORANGE);
            // get_data_vector(VECTOR_MAGNETOMETER, &magn_vector);
            blink(K_LED_ORANGE);
            get_data_vector(VECTOR_GYROSCOPE, &gyro_vector);
            // blink(K_LED_ORANGE);
            // get_data_vector(VECTOR_EULER, &eul_vector);
            // blink(K_LED_ORANGE);
            // get_data_vector(VECTOR_LINEARACCEL, &linear_vector);
            // blink(K_LED_ORANGE);
            // get_data_vector(VECTOR_GRAVITY, &gravity_vector);
        }
        // sprintf(msg,"%d||%3.2f||%3.2f||%1.5f||%1.5f||%2.3f||%3.3f||%3.3f||%2.3f||%2.3f\r\n",
        //     time_ms, temp, hum, pos_vector, accel_vector, magn_vector,
        //     gyro_vector, eul_vector, linear_vector, gravity_vector
        // );
        sprintf(msg, "%d|%3.2f|%3.2f|%1.5f|%3.3f\r\n",
            time_ms, temp, hum, pos_vector, gyro_vector
        );
        k_uart_write(K_UART_CONSOLE, msg, strlen(msg));
        blink(K_LED_RED);
        //vTaskDelay(1000);
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
