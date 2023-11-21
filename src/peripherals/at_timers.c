// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include "peripherals/at_timers.h"
#include "peripherals/at_button.h"
#include "asset_tracker.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_timers, CONFIG_TRACKER_LOG_LEVEL);

static void scan_timer_cb(struct k_timer *timer_id);
static void ble_conn_timer_cb(struct k_timer *timer_id);
static void btn_press_timer_cb(struct k_timer *timer_id);

K_TIMER_DEFINE(scan_timer, scan_timer_cb, NULL);
K_TIMER_DEFINE(ble_conn_timer, ble_conn_timer_cb, NULL);
K_TIMER_DEFINE(btn_press_timer, btn_press_timer_cb, NULL);

bool ble_timeout = false;

static void scan_timer_cb(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);

	//start stack and attempt uplink
	at_event_send(EVENT_SID_START);

	//push sensor scan event
	at_event_send(EVENT_SCAN_SENSORS);
	//push location scan event
	at_event_send(EVENT_SCAN_LOC);

	//reload scan timer
    scan_timer_set_and_run(K_MSEC(300000));  //5min uplink

	// k_timeout_t delay = K_MSEC(CONFIG_SM_TIMER_DEMO_CAPABILITY_PERIOD_MS);
	// enum event_type event = EVENT_NOTIFICATION_TIMER_FIRED;

	// if (BUILT_IN_LM == SID_LINK_TYPE_1) {
	// 	if (!sm_is_sidewalk_ready()) {
	// 		delay = K_MSEC(CONFIG_SM_TIMER_CONNECT_LINK_TYPE_1_DELAY_MS);
	// 		event = EVENT_CONNECT_LINK_TYPE_1;
	// 	}
	// }

	// if (sm_app_state_get() == DEMO_APP_STATE_NOTIFY_SENSOR_DATA && sm_is_sidewalk_ready()) {
	// 	delay = K_MSEC(CONFIG_SM_TIMER_DEMO_NOTIFY_SENSOR_DATA_PERIOD_MS);
	// }
	// k_timer_start(&cap_timer, delay, Z_TIMEOUT_NO_WAIT);

	// sm_main_task_msg_q_write(event);
}

static void ble_conn_timer_cb(struct k_timer *timer_id) {
	ARG_UNUSED(timer_id);
	ble_timeout = true;
	LOG_WRN("BLE connection timeout... uplink failed.");

}

void ble_conn_timer_set_and_run() {
	ble_timeout = false;
	k_timer_start(&ble_conn_timer, K_SECONDS(CONFIG_BLE_CONN_TIMEOUT), Z_TIMEOUT_NO_WAIT);
}

void ble_conn_timer_stop() {
	k_timer_stop(&ble_conn_timer);
}

static void btn_press_timer_cb(struct k_timer *timer_id)
{
	ARG_UNUSED(timer_id);
	button_long_press = true;

}

void scan_timer_set_and_run(k_timeout_t delay)
{
	k_timer_start(&scan_timer, delay, Z_TIMEOUT_NO_WAIT);
}

void btn_press_timer_set_and_run(void)
{
	button_long_press = false;
	k_timer_start(&btn_press_timer, K_MSEC(CONFIG_LONG_PRESS_PER_MS), Z_TIMEOUT_NO_WAIT);
}

void btn_press_timer_stop()
{
	k_timer_stop(&btn_press_timer);
}

unsigned gnss_scan_timer_get()
{
	return k_ticks_to_ms_floor32(scan_timer.period.ticks) / MSEC_PER_SEC;
}

int gnss_scan_timer_set(unsigned sec)
{
	if (sec == 0) {
		k_timer_stop(&scan_timer);
		LOG_DBG("GNSS timer stopped");
	} else {
		k_timer_start(&scan_timer, Z_TIMEOUT_NO_WAIT, K_SECONDS(sec));
	}
	return 0;
}

