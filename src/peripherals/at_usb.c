// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include "peripherals/at_usb.h"

#include <zephyr/kernel.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/usbd.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_usb, CONFIG_TRACKER_LOG_LEVEL);


int init_at_usb(void) {

    if (usb_enable(NULL)) {
		LOG_ERR("USB enabled failed.");
		return -1;
	} else {
		return 0;
	}
}