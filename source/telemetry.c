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
#include <kubos-core/modules/sensors/htu21d.h>
#include <kubos-core/modules/sensors/bno055.h>
#include <kubos-hal/gpio.h>
#include <telemetry/config.h>
#include <telemetry-aggregator/aggregator.h>


/* Setup telemetry sources */
static telemetry_source temp_source = { .source_id = 0, .data_type = TELEMETRY_TYPE_INT };
static telemetry_source hum_source  = { .source_id = 1, .data_type = TELEMETRY_TYPE_INT };

static void htu_aggregator()
{
    float temp = 0;
    float hum = 0;

    htu21d_setup();
    htu21d_reset();

    htu21d_read_temperature(&temp);
    aggregator_submit(temp_source, temp);

    htu21d_read_humidity(&hum);
    aggregator_submit(hum_source, hum);
}


static void bno_aggregator()
{
    bno055_quat_data_t quat_data;
    bno055_vector_data_t eul_vector;
    bno055_vector_data_t grav_vector;
    bno055_vector_data_t lin_vector;
    bno055_vector_data_t acc_vector;
    static KI2CStatus bno_stat;

    csp_mutex_lock(&bno_lock, CSP_MAX_DELAY);

    bno_stat = bno055_setup(OPERATION_MODE_NDOF);
    load_calibration();

    blink(K_LED_ORANGE);
    bno055_get_position(&quat_data);
    telemetry_source quat_w = { .source_id = 2, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(quat_w, quat_data.w);
    telemetry_source quat_x = { .source_id = 3, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(quat_x, quat_data.x);
    telemetry_source quat_y = { .source_id = 4, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(quat_y, quat_data.y);
    telemetry_source quat_z = { .source_id = 5, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(quat_z, quat_data.z);

    blink(K_LED_ORANGE);
    bno055_get_data_vector(VECTOR_EULER, &eul_vector);
    telemetry_source eul_x = { .source_id = 6, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(eul_x, eul_vector.x);
    telemetry_source eul_y = { .source_id = 7, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(eul_y, eul_vector.y);
    telemetry_source eul_z = { .source_id = 8, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(eul_z, eul_vector.z);

    blink(K_LED_ORANGE);
    bno055_get_data_vector(VECTOR_GRAVITY, &grav_vector);
    telemetry_source grav_vector_x = { .source_id = 9, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(grav_vector_x, grav_vector.x);
    telemetry_source grav_vector_y = { .source_id = 10, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(grav_vector_y, grav_vector.y);
    telemetry_source grav_vector_z = { .source_id = 11, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(grav_vector_z, grav_vector.z);

    blink(K_LED_ORANGE);
    bno055_get_data_vector(VECTOR_LINEARACCEL, &lin_vector);
    telemetry_source lin_vector_x = { .source_id = 12, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(lin_vector_x, lin_vector.x);
    telemetry_source lin_vector_y = { .source_id = 13, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(lin_vector_y, lin_vector.y);
    telemetry_source lin_vector_z = { .source_id = 14, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(lin_vector_z, lin_vector.z);

    blink(K_LED_ORANGE);
    bno055_get_data_vector(VECTOR_ACCELEROMETER, &acc_vector);
    telemetry_source acc_vector_x = { .source_id = 15, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(acc_vector_x, acc_vector.x);
    telemetry_source acc_vector_y = { .source_id = 16, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(acc_vector_y, acc_vector.y);
    telemetry_source acc_vector_z = { .source_id = 17, .data_type = TELEMETRY_TYPE_FLOAT };
    aggregator_submit(acc_vector_z, acc_vector.z);

    csp_mutex_unlock(&bno_lock);
}

/**
 * Implementing user_aggregator function defined by telemetry-aggregator module.
 * This function is defined in <telemetry-aggregator/aggregator.h>
 */
void user_aggregator()
{
    htu_aggregator();
    bno_aggregator();
}
