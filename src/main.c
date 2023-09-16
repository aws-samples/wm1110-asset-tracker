// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include "asset_tracker.h"

#include <zephyr/kernel.h>

#include "peripherals/at_led.h"
#include "peripherals/at_button.h"
#include "peripherals/at_sht41.h"
#include "peripherals/at_lis3dh.h"
#include "peripherals/at_usb.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_TRACKER_LOG_LEVEL);

/* 1000 msec = 1 sec */
#define SLEEP_TIME_MS   1000

int main(void)
{
	init_at_usb();
	#if defined(CONFIG_LOG)
	k_msleep(2000);  //wait for a couple of sec for the serial terminal to reconnect
	#endif

	init_at_led();
	init_at_button();
	init_at_sht41();
	init_at_lis3dh();

	LOG_INF("Starting Sidewalk Asset Tracker...");

	if (at_thread_init()) {
		LOG_ERR("Failed to start asset tracker thread");
	}

	while (1) {
		at_led_toggle();
		k_msleep(SLEEP_TIME_MS);
	}
	return 0;
}
