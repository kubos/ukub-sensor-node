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
#include <telemetry/destinations.h>
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
void csp_uart_sender(void *p) {

    /* Create socket without any socket options */
    csp_socket_t *sock = csp_socket(CSP_SO_NONE);

    /* Bind all ports to socket */
    csp_bind(sock, CSP_ANY);

    /* Create 10 connections backlog queue */
    csp_listen(sock, 10);

    /* Pointer to current connection and packet */
    csp_conn_t *incoming_connection; //the incoming connection from Telemetry
    csp_conn_t *outgoing_connection; //the outgoing connection over UART
    csp_packet_t *packet;
    telemetry_packet* t_packet;

    /* Process incoming connections */
    while (1) {

        /* Wait for connection, 100 ms timeout */
        if ((incoming_connection = csp_accept(sock, 100)) == NULL)
            continue;

        /* Read packets. Timout is 100 ms */
        while ((packet = csp_read(incoming_connection, 100)) != NULL) {
            switch (csp_conn_dport(incoming_connection)) {
                //This is a switch in case we want to use one of the other ports in the future
                case TELEMETRY_HEALTH_PORT:
                    blink(K_LED_BLUE);
                    outgoing_connection = csp_connect(CSP_PRIO_NORM, OUTPUT_ADDRESS, TELEMETRY_BEACON_PORT, 100, CSP_O_NONE);
                    if (outgoing_connection == NULL) {
                        /* Connect failed */
                        /* Remember to free packet buffer */
                        csp_buffer_free(packet);
                        return;
                    }
                    /* Send packet */
                    if (!csp_send(outgoing_connection, packet, 100)) {
                        /* Send failed */
                        blink(K_LED_RED);
                    }
                    csp_buffer_free(packet);
                    break;
            }
        }

        /* Close current connection, and handle next */
        csp_close(incoming_connection);
        csp_close(outgoing_connection);
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

    /* Init telemetry-aggregator thread */
    INIT_AGGREGATOR_THREAD;

    /* Init calibration thread */
    csp_thread_handle_t calib_handle;
    csp_thread_create(calibrate_thread, "CALIBRATE", 1024, NULL, 0, &calib_handle);
    /* Init the csp -> uart thread */
    csp_thread_create(csp_uart_sender, "CSP_SENDER", 1024, NULL, 0, NULL);

    vTaskStartScheduler();

    return 0;
}
