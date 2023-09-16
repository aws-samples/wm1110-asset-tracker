// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>

#include "asset_tracker.h"
#include "peripherals/at_lis3dh.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_lis3dh, CONFIG_TRACKER_LOG_LEVEL);

#if !DT_HAS_COMPAT_STATUS_OKAY(st_lis3dh)
#error "No st_lis3dh compatible node found in the device tree"
#endif
/*
 * A build error on this line means your board is unsupported.
 * See the sample documentation for information on how to fix this.
 */

static const struct device *const acceld = DEVICE_DT_GET(DT_ALIAS(accel0));


int init_at_lis3dh(void) {
	
	if (!device_is_ready(acceld)) {
		LOG_ERR("LIS3DH device not ready.");
		return -1;
   	} else {
		return 0;
	}
}

int get_accel(struct at_sensors *sensors) {

	struct sensor_value accel[3];
	const char *overrun = "";
	int rc = sensor_sample_fetch(acceld);

	if (rc == -EBADMSG) {
		/* Sample overrun.  Ignore in polled mode. */
		if (IS_ENABLED(CONFIG_LIS2DH_TRIGGER)) {
			overrun = "[OVERRUN] ";
		}
		rc = 0;
	}
	if (rc == 0) {
		rc = sensor_channel_get(acceld,
					SENSOR_CHAN_ACCEL_XYZ,
					accel);
	}

	sensors->max_accel_x = sensor_value_to_double(&accel[0]);
	sensors->max_accel_y = sensor_value_to_double(&accel[1]);
	sensors->max_accel_z = sensor_value_to_double(&accel[2]);

	//find the max of the three
	sensors->peak_accel = sensors->max_accel_x;
	if(sensors->peak_accel < sensors->max_accel_y) {
		sensors->peak_accel = sensors->max_accel_y;
	}
	if(sensors->peak_accel < sensors->max_accel_z) {
		sensors->peak_accel = sensors->max_accel_z;
	}

	if (rc < 0) {
		LOG_ERR("ERROR: Update failed: %d", rc);
	} else {
		LOG_DBG("%sx %f , y %f , z %f",
		       overrun,
		       sensors->max_accel_x,
		       sensors->max_accel_y,
		       sensors->max_accel_z);
	}

	return 0;

}