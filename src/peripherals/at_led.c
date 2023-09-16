// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include "peripherals/at_led.h"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_led, CONFIG_TRACKER_LOG_LEVEL);

/* The devicetree node identifier for the "led0" alias. */
#define LED0_NODE DT_ALIAS(led0)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);

int init_at_led(void) {
	if (!gpio_is_ready_dt(&led)) {
		LOG_ERR("led device not ready.");
		return -1;
	}
	
	gpio_pin_configure_dt(&led, GPIO_OUTPUT_ACTIVE);

	at_led_off();

	return 0;
}

void at_led_off(void) {
    gpio_pin_set_dt(&led, 0);
}

void at_led_on(void) {
    gpio_pin_set_dt(&led, 1);
}

void at_led_toggle(void) {
    gpio_pin_toggle_dt(&led);
}


