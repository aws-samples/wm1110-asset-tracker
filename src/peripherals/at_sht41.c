// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/sensor/sht4x.h>

#include "asset_tracker.h"
#include "peripherals/at_sht41.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_sht41, CONFIG_TRACKER_LOG_LEVEL);

#if !DT_HAS_COMPAT_STATUS_OKAY(sensirion_sht4x)
#error "No sensirion,sht4x compatible node found in the device tree"
#endif
/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */

static const struct device *const th_sensor = DEVICE_DT_GET_ONE(sensirion_sht4x);


int init_at_sht41(void) {

	if (!device_is_ready(th_sensor)) {
		LOG_ERR("SHT41 device not ready.");
		return -1;
   	} else {
		return 0;
	}
}

int get_temp_hum(struct at_sensors *sensors) {

	struct sensor_value temp, hum;

	if (sensor_sample_fetch(th_sensor)) {
		LOG_ERR("Failed to fetch sample from SHT4X device\n");
		return -1;
	}

	sensor_channel_get(th_sensor, SENSOR_CHAN_AMBIENT_TEMP, &temp);
	sensor_channel_get(th_sensor, SENSOR_CHAN_HUMIDITY, &hum);


	sensors->temp = sensor_value_to_double(&temp);
	sensors->hum = sensor_value_to_double(&hum);

	LOG_INF("SHT4X: %.2f Temp. [C] ; %0.2f RH [%%]", sensors->temp, sensors->hum); 

	return 0;
}