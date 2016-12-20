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
#include <csp/drivers/usart.h>
#include <csp/interfaces/csp_if_kiss.h>

#include <kubos-hal/gpio.h>
#include <kubos-hal/uart.h>
#include <telemetry/config.h>
#include <telemetry-aggregator/aggregator.h>

//TODO: Move this to the config file
#define OUTPUT_ADDRESS 3

static csp_iface_t csp_if_kiss;
static csp_kiss_handle_t csp_kiss_driver;


/* ---------------------- */
/* telemetry => UART task */
/* ---------------------- */
//TODO: Put this function somewhere else
//TODO: Come up with a better name for this function
void csp_uart_sender(void *p)
{

    /* Create socket without any socket options */
    csp_socket_t *sock = csp_socket(CSP_SO_NONE);

    /* Bind all ports to socket */
    csp_bind(sock, CSP_ANY);

    /* Create 10 connections backlog queue */
    csp_listen(sock, 10);

    /* Pointer to current connection and packet */
    csp_conn_t *output_connection; //the outgoing connection over UART
    csp_packet_t* csp_packet;
    telemetry_packet read_packet;
    uint8_t mask = 0xFF;
    telemetry_conn tel_conn;
    while (!telemetry_subscribe(&tel_conn, mask))
    {
        csp_sleep_ms(500);
    }

    /* Process incoming connections */
     while (1)
     {
         if(telemetry_read(tel_conn, &read_packet))
         {
            /* A packet has been read in from telemetry */
            csp_packet = csp_buffer_get(20);
            if (csp_packet != NULL)
            {
                memcpy(csp_packet->data, &read_packet, sizeof(telemetry_packet));
                csp_packet->length = sizeof(telemetry_packet);
                output_connection = csp_connect(CSP_PRIO_NORM, OUTPUT_ADDRESS, TELEMETRY_CSP_PORT, 100, CSP_O_NONE);

                if (output_connection != NULL)
                {
                    csp_send(output_connection, csp_packet, 100);
                    csp_buffer_free(csp_packet);
                }
            }
        }
        csp_close(output_connection);
     }
}


void local_usart_rx(uint8_t * buf, int len, void * pxTaskWoken) {
    csp_kiss_rx(&csp_if_kiss, buf, len, pxTaskWoken);
}


/* ------------ */
/*     Main     */
/* ------------ */
int main(void) {

    k_uart_console_init();

    /* Do all of the CSP setup things*/
    struct usart_conf conf;
    char dev = (char)K_UART6;
    conf.device = &dev;
    conf.baudrate = K_UART_CONSOLE_BAUDRATE;
    usart_init(&conf);

    k_gpio_init(K_LED_GREEN, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_ORANGE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_RED, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);
    k_gpio_init(K_LED_BLUE, K_GPIO_OUTPUT, K_GPIO_PULL_NONE);

    /* init kiss interface */
    csp_kiss_init(&csp_if_kiss, &csp_kiss_driver, usart_putc, usart_insert, "KISS");

    /* Setup callback from USART RX to KISS RS */
    /* This is needed if we use CSP's RDP packets - Otherwise we don't need it as we're only sending, not receiving packets */
    usart_set_callback(local_usart_rx);

    /*set the output_address route to use the kiss interface */
    csp_route_set(OUTPUT_ADDRESS, &csp_if_kiss, CSP_NODE_MAC);

    /**
     * Init CSP bits for telemetry to use - Wrap this up somewhere
     */
    csp_buffer_init(5, 20);
    csp_init(TELEMETRY_CSP_ADDRESS);
    csp_route_start_task(50, 1);

    telemetry_init();

    /* Init telemetry-aggregator thread */
    INIT_AGGREGATOR_THREAD;
    /* Init calibration thread */
    csp_thread_handle_t calib_handle;
    /*csp_thread_create(calibrate_thread, "CALIBRATE", 1024, NULL, 0, &calib_handle);*/
    /* Init the csp -> uart thread */
    csp_thread_handle_t handle_csp_uart_sender;
    csp_thread_create(csp_uart_sender, "CSP_SENDER", 1000, NULL, 0, &handle_csp_uart_sender);

    vTaskStartScheduler();
    while (1) {
        csp_sleep_ms(100000);
    }

    return 0;
}

