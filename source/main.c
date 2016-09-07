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

/**
 * This code specifically interacts with the Bosch BNO055 
 * "Intelligent 9-axis absolute orientation sensor"
 * (inertial movement unit / magnetometer / gyroscope)
 * and is written against the November 2014 (version 1.2) documentation.
 *
 * 
 * Throughout this code, comments that are not meant to be machine-
 * readable data are made with leading double-splats (asterisks)
 * for ease of extraction.
 *
 * 
 * This code assumes that the BNO055 is mounted in the standard
 * orientation as specified in the Bosch datasheet. That is, 
 * AXIS_REMAP_CONFIG is set to P1 (0x24), and 
 * AXIS_REMAP_SIGN is st to 0x00. Other mounting configurations are 
 * supported. See 
 * kubos-core/source/modules/sensors/bno055.c
 * and 
 * kubos-core/modules/sensors/bno055.h
 */ 


/* ------------ */
/*  Functions   */
/* ------------ */


/** 
 * The displayCalStatus function checks calibration status and either
 * prints an error statement to UART or else prints the current calibration 
 * values to UART in a bar- and tab-separated text format.
 */

void displayCalStatus(void)
{

/* Get the four calibration values (0..3) */ 

/** 
 * When retrieving the four calibration values (2 bit, 0..3),
 * Any sensor data reporting 0 should be ignored, and
 * 3 means 'fully calibrated". 
 *
 * KSensorStatus is described in kubos-core/modules/sensors/sensors.h
 */

    bno055_calibration_data_t calib;
	KSensorStatus status;

    status = bno055_get_calibration(&calib);

    if( status != SENSOR_OK)
    {
    	printf("** Couldn't get calibration values! Status=%d\r\n", status);
    }

/* Display the individual values */
    printf("S:\t%d\tG:\t%d %d|%d|%d\tA:\t%di\t%d|%d|%d|%d\tM:\t%d %d|%d|%d|%d\r\n",
        calib.sys,
		calib.gyro, offsets.gyro_offset_x, offsets.gyro_offset_y, offsets.gyro_offset_z,
		calib.accel, offsets.accel_offset_x, offsets.accel_offset_y, offsets.accel_offset_z, offsets.accel_radius,
		calib.mag, offsets.mag_offset_x, offsets.mag_offset_y, offsets.mag_offset_z, offsets.mag_radius
    );
}


/** 
 * The open_file function ( from the FatFS library) opens the calibration 
 * profile file, if one exists.
 * @param Fil a pointer to the file object structure
 * @param settings a string of mode flags, a list of which can be found at 
 * http://elm-chan.org/fsw/ff/en/open.html
 * @return result a table of values which (0 being 'okay') is found at
 * http://elm-chan.org/fsw/ff/en/rc.html  
 *
 */

uint16_t open_file(FIL * Fil, char settings)
{
    uint16_t result;
    char * path = "CALIB.txt";

	result = f_open(Fil, path, settings);
	printf("** Opened file: %d\r\n", result);

    return result;
}


/** 
 * The close_file function closes a file and releases the handle.
 * @param Fil a pointer to the file object structure
 * @return ret a table of values which (0 being 'okay') is found at
 * http://elm-chan.org/fsw/ff/en/rc.html  
 */

uint16_t close_file(FIL * Fil)
{
	uint16_t ret;
	ret = f_close(Fil);
	printf("** Close RC: %d\r\n", ret);
	return ret;
}


/**
 * The read_value function checks for an available string to read
 * from the calibration profile file and, if it is available, reads the 
 * string.
 * 
 * @param Fil,  pointer to the file object structure
 * @return ret, 0 being 'FR_OK') 
 * and -1 being either 'at the end of tile' or 'this thing is not a digit'.
 */

uint16_t read_value(FIL * Fil, uint16_t * value)
{
	uint16_t ret = FR_OK;
	uint16_t temp = 0;
	int c;
	char buffer[128];

/* Make sure there's something to read */
	if(f_eof(Fil))
	{
		return -1;
	}

	f_gets(buffer, sizeof buffer, Fil);

	if(!isdigit(buffer[0]))
	{
		return -1;
	}

/* convert read string to uint */
	for (c = 0; isdigit(buffer[c]); c++)
	{
		temp = temp * 10 + buffer[c] - '0';
	}

	*value = temp;

	return ret;
}


/** 
 * write_value: a function to write a calibration profile value to the file.
 * If successful, the green LED blinks.
 * @param Fil, pointer to the file object structure
 * @param value, the thing you want to write to the file
 * @return ret, a table of values which (0 being 'okay') is found at
 * http://elm-chan.org/fsw/ff/en/rc.html  
 */

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

/** 
 * load_calibration: a function to load the calibration profile.
 * If it's the first time loading a value, mount the file system too.
 * @return ret, 
 */ 

KSensorStatus load_calibration(void)
{

	KSensorStatus ret = SENSOR_ERROR;
	static FATFS FatFs;
	static FIL Fil;
	uint16_t sd_stat = FR_OK;

/* Mount the file system if needed. */
	if(!offsets_set)
	{
		sd_stat = f_mount(&FatFs, "", 1);
	}

	offsets_set = false;

	if(sd_stat == FR_OK)
	{
/* Open the calibration file */
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
				printf("** Loaded calibration from SD card\r\n");
				offsets_set = true;
			}

			sd_stat = close_file(&Fil);

		}
	}

/** 
 * The code is set to provide default calibration values three primary 
 * sensors (three axes each for the accelerometer, gyroscope, and 
 * magnetometer, plus a radius value for the accelerometer and magnetometer).
 */

	if(!offsets_set)
	{
		printf("** Loading default calibration values\r\n");

/* Load values into offset structure */
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

/* Set the values */
	ret = bno055_set_sensor_offset_struct(offsets);

	return ret;
}


/** 
 * save_calibration: push the calibration values to a file on the uSD card.
 * @param calib the bno055_offsets_t struct that stores calibration values
 */

void save_calibration(bno055_offsets_t calib)
{
	static FATFS FatFs;
	static FIL Fil;
	uint16_t sd_stat = FR_OK;

/* Open calibration file */
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
			printf("** Saved calibration to SD card\r\n");
		}

		close_file(&Fil);
	}

}

/** 
 * task_sensors: the primary task for sensor operation.
 *  
 */

void task_sensors(void *p)
{
    float temp = 0;
    float hum = 0;
    bno055_quat_data_t quat_data;
    bno055_vector_data_t eul_vector;
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
//   get_position(&pos_vector);
    blink(K_LED_ORANGE);
    htu21d_read_temperature(&temp);
    blink(K_LED_ORANGE);
    htu21d_read_humidity(&hum);

/**
 * the Orange LED (on the MicroPython board) blinks orange when reading the
 * temperature value. 
 */

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

/* Check status of calibration */
            oldCount = calibCount;
            if(bno055_check_calibration(&calibCount, 5, &offsets) != SENSOR_OK)
            {
/* Reload the calibration profile */
            	if(calibCount == 0)
    			{
            		printf("** Reloading calibration profile\r\n");
            		load_calibration();
            	}

/**
 * While the calibration profile is being loaded, the red LED will blink 
 * in three-blink groups. 
 */

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

/**
 * If the I2C setup is okay and the sensor is calibrated, the system will
 * blink the orange LED as it gets the quaternions data and the
 * Euler angles (here again, note the sensor is expected to be in 
 * "fusion mode" and not gathering raw inputs).
 *
 * Next, display the calibration status.
 * 
 * Then print out the resulting data separated by bars.
 * Upon writing the string to the UART, the green LED will blink.
 */


        if (bno_stat != I2C_OK && bno_stat != SENSOR_NOT_CALIBRATED)
        {
        	printf("** bno_stat = %d\r\n", bno_stat);
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

        }

        displayCalStatus();

        sprintf(msg, "%d|%3.2f|%3.2f|%f|%f|%f|%f|%f|%f|%f\r\n",
            time_ms, temp, hum,
            quat_data.w, quat_data.x, quat_data.y, quat_data.z,
            eul_vector.x, eul_vector.y, eul_vector.z
        );
        k_uart_write(K_UART_CONSOLE, msg, strlen(msg));
        blink(K_LED_GREEN);
    }
}


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

    xTaskCreate(task_sensors, "sensors", configMINIMAL_STACK_SIZE * 4, NULL, 2, NULL);

    vTaskStartScheduler();

    return 0;
}
