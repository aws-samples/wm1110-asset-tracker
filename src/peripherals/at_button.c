// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#include "peripherals/at_button.h"
#include "peripherals/at_timers.h"
#include "asset_tracker.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_button, CONFIG_TRACKER_LOG_LEVEL);

static const struct gpio_dt_spec userbutton = GPIO_DT_SPEC_GET(DT_ALIAS(button0), gpios);
static struct gpio_callback userbutton_cb_data;
bool userbutton_pressed = false;
bool button_long_press;

void button_pressed_cb(const struct device *dev, struct gpio_callback *cb, gpio_port_pins_t pins)
{

	ARG_UNUSED(dev);
	ARG_UNUSED(cb);
	ARG_UNUSED(pins);

	if (gpio_pin_get_dt(&userbutton)) {
		LOG_INF("userbutton pressed");
		userbutton_pressed = true;
		btn_press_timer_set_and_run();

		// at_event_send(EVENT_WIFI_SCAN);
	} else {
		btn_press_timer_stop();
		userbutton_pressed = false;

		if(button_long_press) {
			LOG_INF("userbutton LONG press");
			//switch link mode type and reset
		} else {
			LOG_INF("Immediate scan and uplink triggered...");
			scan_timer_set_and_run(K_MSEC(500));
		}
	}

}

int init_at_button(void) {
	if (!gpio_is_ready_dt(&userbutton)) {
		LOG_ERR("userbutton device not ready.");
		return -1;
	}

	if (gpio_pin_configure_dt(&userbutton, GPIO_INPUT) != 0) {
		LOG_ERR("userbutton gpio pin configure failed.");
		return -1;
	}

	if (gpio_pin_interrupt_configure_dt(&userbutton, GPIO_INT_EDGE_BOTH) != 0) {
		LOG_ERR("userbutton interrupt configure failed.");
		return -1;
	}

	gpio_init_callback(&userbutton_cb_data, button_pressed_cb, BIT(userbutton.pin));

	if (gpio_add_callback(userbutton.port, &userbutton_cb_data) != 0) {
		LOG_ERR("button1 gpio add callback failed.");
		return -1;
	}

	LOG_INF("init complete.");
	return 0;
}
