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
#include "disk.h"

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
	//printf("** Opened file: %d\r\n", result);

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
        ret = 0;
    }

    return ret;
}
