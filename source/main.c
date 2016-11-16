/*
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
#include "misc.h"
#include "sensor.h"

#include <csp/csp.h>
#include <kubos-hal/gpio.h>
#include <kubos-hal/uart.h>
#include <telemetry/destinations.h>
#include <telemetry-aggregator/aggregator.h>


/* ------------ */
/*     Main     */
/* ------------ */
int main(void)
{
    k_uart_console_init();

    k_gpio_init(K_LED_GREEN, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_ORANGE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_RED, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_BLUE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);

    /**
     * Init CSP bits for telemetry to use - Wrap this up somewhere
     */
    csp_buffer_init(5, 20);
    csp_init(TELEMETRY_CSP_ADDRESS);
    csp_route_start_task(50, 1);

    /* Init telemetry-aggregator thread */
    INIT_AGGREGATOR_THREAD;

    /* Init calibration thread */
    csp_thread_handle_t calib_handle;
    csp_thread_create(calibrate_thread, "CALIBRATE", 1024, NULL, 0, &calib_handle);

    vTaskStartScheduler();

    return 0;
}
