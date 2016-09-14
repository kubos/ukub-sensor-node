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

#include "kubos-core/modules/fs/fs.h"
#include "kubos-core/modules/fatfs/ff.h"
#include "kubos-core/modules/fatfs/diskio.h"

#include <csp/csp.h>

static bno055_offsets_t offsets;
static bool offsets_set;

static inline void blink(int pin) {
    #define BLINK_MS 100
    k_gpio_write(pin, 1);
    vTaskDelay(1);
    k_gpio_write(pin, 0);
}

//Display current calibration status
void displayCalStatus(void)
{
    /* Get the four calibration values (0..3) */
    /* Any sensor data reporting 0 should be ignored, */
    /* 3 means 'fully calibrated" */
    bno055_calibration_data_t calib;
	KSensorStatus status;

    status = bno055_get_calibration(&calib);

    if( status != SENSOR_OK)
    {
    	printf("Couldn't get calibration values! Status=%d\r\n", status);
    }

    /* Display the individual values */
    printf("S: %d G: %d A: %d M: %d \r\n",
        calib.sys,
		calib.gyro,
		calib.accel,
		calib.mag
    );
}

//Open calibration profile file
uint16_t open_file(FIL * Fil, char settings)
{
    uint16_t result;
    char * path = "CALIB.txt";

	result = f_open(Fil, path, settings);
	printf("Opened file: %d\r\n", result);

    return result;
}

uint16_t close_file(FIL * Fil)
{
	uint16_t ret;
	ret = f_close(Fil);
	return ret;
}

//Read the next value from the calibration profile file
uint16_t read_value(FIL * Fil, uint16_t * value)
{
	uint16_t ret = FR_OK;
	uint16_t temp = 0;
	int c;
	char buffer[128];

	//Make sure there's something to read
	if(f_eof(Fil))
	{
		return -1;
	}

	f_gets(buffer, sizeof buffer, Fil);

	if(!isdigit(buffer[0]))
	{
		return -1;
	}

	//convert read string to uint
	for (c = 0; isdigit(buffer[c]); c++)
	{
		temp = temp * 10 + buffer[c] - '0';
	}

	*value = temp;

	return ret;
}

//Write a calibration profile value to the file
uint16_t write_value(FIL * Fil, uint16_t value)
{
    uint16_t ret;
    uint16_t bw;
    if ((ret = f_printf(Fil, "%d\n", value)) != -1)
    {
        blink(K_LED_GREEN);
        ret = 0;
    }

    return ret;
}

//Load the calibration profile
KSensorStatus load_calibration(void)
{

	KSensorStatus ret = SENSOR_ERROR;
	static FATFS FatFs;
	static FIL Fil;
	uint16_t sd_stat = FR_OK;

	//If it's our first time loading, mount the file system
	if(!offsets_set)
	{
		sd_stat = f_mount(&FatFs, "", 1);
	}

	offsets_set = false;

	if(sd_stat == FR_OK)
	{
		//Open the calibration file
		if((sd_stat = open_file(&Fil, FA_READ | FA_OPEN_EXISTING)) == FR_OK)
		{
			sd_stat = read_value(&Fil, &offsets.accel_offset_x);
			sd_stat |= read_value(&Fil, &offsets.accel_offset_y);
			sd_stat |= read_value(&Fil, &offsets.accel_offset_z);
			sd_stat |= read_value(&Fil, &offsets.accel_radius);

			sd_stat |= read_value(&Fil, &offsets.gyro_offset_x);
			sd_stat |= read_value(&Fil, &offsets.gyro_offset_y);
			sd_stat |= read_value(&Fil, &offsets.gyro_offset_z);

			sd_stat |= read_value(&Fil, &offsets.mag_offset_x);
			sd_stat |= read_value(&Fil, &offsets.mag_offset_y);
			sd_stat |= read_value(&Fil, &offsets.mag_offset_z);
			sd_stat |= read_value(&Fil, &offsets.mag_radius);

			if(sd_stat == FR_OK)
			{
				printf("Loaded calibration from SD card\r\n");
				offsets_set = true;
			}

			sd_stat = close_file(&Fil);

		}
	}

	if(!offsets_set)
	{
		printf("Loading default calibration values\r\n");

		//Load values into offset structure
		offsets.accel_offset_x = 65530;
		offsets.accel_offset_y = 81;
		offsets.accel_offset_z = 27;
		offsets.accel_radius = 1000;

		offsets.gyro_offset_x = 0;
		offsets.gyro_offset_y = 0;
		offsets.gyro_offset_z = 0;

		offsets.mag_offset_x = 65483;
		offsets.mag_offset_y = 5;
		offsets.mag_offset_z = 76;
		offsets.mag_radius = 661;

		offsets_set= true;
	}

	//Set the values
	ret = bno055_set_sensor_offset_struct(offsets);

	return ret;
}

void save_calibration(bno055_offsets_t calib)
{
	static FATFS FatFs;
	static FIL Fil;
	uint16_t sd_stat = FR_OK;

	//Open calibration file
	if((sd_stat = open_file(&Fil, FA_WRITE | FA_OPEN_ALWAYS)) == FR_OK)
	{
		sd_stat = write_value(&Fil, calib.accel_offset_x);
		sd_stat |= write_value(&Fil, calib.accel_offset_y);
		sd_stat |= write_value(&Fil, calib.accel_offset_z);
		sd_stat |= write_value(&Fil, calib.accel_radius);

		sd_stat |= write_value(&Fil, calib.gyro_offset_x);
		sd_stat |= write_value(&Fil, calib.gyro_offset_y);
		sd_stat |= write_value(&Fil, calib.gyro_offset_z);

		sd_stat |= write_value(&Fil, calib.mag_offset_x);
		sd_stat |= write_value(&Fil, calib.mag_offset_y);
		sd_stat |= write_value(&Fil, calib.mag_offset_z);
		sd_stat |= write_value(&Fil, calib.mag_radius);

		if(sd_stat == FR_OK)
		{
			printf("Saved calibration to SD card\r\n");
		}

		close_file(&Fil);
	}

}

void task_sensors(void *p)
{
    float temp = 0;
    float hum = 0;
    bno055_quat_data_t quat_data;
    bno055_vector_data_t eul_vector;
    bno055_vector_data_t grav_vector;
    bno055_vector_data_t lin_vector;
    bno055_vector_data_t acc_vector;
    uint32_t time_ms;
    static char msg[255];
    uint8_t count = 1;
    uint8_t calibCount = 0;
    uint8_t oldCount = 0;

    static KI2CStatus bno_stat;

    htu21d_setup();
    htu21d_reset();
    bno_stat = bno055_setup(OPERATION_MODE_NDOF);
    load_calibration(); //Load bno055 calibration profile
    // get_position(&pos_vector);
    blink(K_LED_ORANGE);
    htu21d_read_temperature(&temp);
    blink(K_LED_ORANGE);
    htu21d_read_humidity(&hum);

    while(1)
    {
        blink(K_LED_ORANGE);
        time_ms = csp_get_ms();
        if ((++count % 30) == 0)
        {
            blink(K_LED_ORANGE);
            htu21d_read_temperature(&temp);
            blink(K_LED_ORANGE);
            htu21d_read_temperature(&temp);
            blink(K_LED_ORANGE);
            displayCalStatus();

            //Check status of calibration
            oldCount = calibCount;
            if(bno055_check_calibration(&calibCount, 5, &offsets) != SENSOR_OK)
            {
            	//Reload the calibration profile
            	if(calibCount == 0)
    			{
            		printf("Reloading calibration profile\r\n");
            		load_calibration();
            	}

            	blink(K_LED_RED);
            	vTaskDelay(100);
            	blink(K_LED_RED);
            	vTaskDelay(100);
            	blink(K_LED_RED);
            }
            else if(oldCount != 0)
            {
            	save_calibration(offsets);
            }

            count = 1;
        }

        if (bno_stat != I2C_OK && bno_stat != SENSOR_NOT_CALIBRATED)
        {
        	printf("bno_stat = %d\r\n", bno_stat);
            blink(K_LED_RED);
            blink(K_LED_RED);
            bno_stat = bno055_setup(OPERATION_MODE_NDOF);
            load_calibration();
        }
        else
        {

            blink(K_LED_ORANGE);
            bno055_get_position(&quat_data);
            blink(K_LED_ORANGE);
            bno055_get_data_vector(VECTOR_EULER, &eul_vector);
            blink(K_LED_ORANGE);
            bno055_get_data_vector(VECTOR_GRAVITY, &grav_vector);
            blink(K_LED_ORANGE);
            bno055_get_data_vector(VECTOR_LINEARACCEL, &lin_vector);
            blink(K_LED_ORANGE);
            bno055_get_data_vector(VECTOR_ACCELEROMETER, &acc_vector);

        }

        sprintf(msg, "%d|%3.2f|%3.2f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f|%f\r\n",
            time_ms, temp, hum,
            quat_data.w, quat_data.x, quat_data.y, quat_data.z,
            eul_vector.x, eul_vector.y, eul_vector.z,
            grav_vector.x, grav_vector.y, grav_vector.z,
            lin_vector.x, lin_vector.y, lin_vector.z,
            acc_vector.x, acc_vector.y, acc_vector.z
        );
        k_uart_write(K_UART_CONSOLE, msg, strlen(msg));
        blink(K_LED_GREEN);
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
