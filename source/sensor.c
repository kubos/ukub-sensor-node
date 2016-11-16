#include "disk.h"
#include "misc.h"
#include "sensor.h"

#include <kubos-hal/gpio.h>
#include <kubos-core/modules/sensors/bno055.h>


static bno055_offsets_t offsets;
static bool offsets_set;

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
    printf("S:\t%d\tG:\t%d\tA:\t%d\tM:\t%d\r\n",
        calib.sys,
		calib.gyro,
		calib.accel,
		calib.mag
    );
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
				//printf("** Loaded calibration from SD card\r\n");
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
		//printf("** Loading default calibration values\r\n");

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
			//printf("** Saved calibration to SD card\r\n");
		}

		close_file(&Fil);
	}
}

CSP_DEFINE_TASK(calibrate_thread)
{
    static bno055_offsets_t offsets;
    static bool offsets_set;
    uint8_t calibCount = 0;
    uint8_t oldCount = 0;

    csp_mutex_create(&bno_lock);

    while(1)
    {
        csp_mutex_lock(&bno_lock, CSP_MAX_DELAY);
        if(bno055_check_calibration(&calibCount, 5, &offsets) != SENSOR_OK)
        {
            /* Reload the calibration profile */
            if(calibCount == 0)
            {
                //printf("** Reloading calibration profile\r\n");
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
        csp_mutex_unlock(&bno_lock);
        csp_sleep_ms(30000);
    }
}