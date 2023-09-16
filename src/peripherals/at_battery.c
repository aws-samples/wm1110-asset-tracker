// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include "asset_tracker.h"
#include "peripherals/at_battery.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_battery, CONFIG_TRACKER_LOG_LEVEL);


int get_batt(struct at_sensors *sensors) {
    //use hardcoded value until battery measurement possbile on HW

    sensors->batt = 95;

    return 0; 
}