// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <sid_api.h>
#include <sid_error.h>
#include <zephyr/kernel.h>
#include <pal_init.h>

#if defined(CONFIG_ASSET_TRACKER_CLI)
#include "at_shell.h"
#endif
#if defined(CONFIG_SIDEWALK_CLI)
#include <sid_shell.h>
#endif
#if defined(CONFIG_LR1110_CLI)
#include <lr1110_shell.h>
#endif
#include <halo_lr11xx_radio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(asset_tracker, CONFIG_TRACKER_LOG_LEVEL);

#include "sidewalk/sidewalk_callbacks.h"
#include <sid_pal_assert_ifc.h>
#include <app_ble_config.h>
#include <app_subGHz_config.h>

#include <asset_tracker.h>
#include "peripherals/at_battery.h"
#include "peripherals/at_lis3dh.h"
#include "peripherals/at_sht41.h"
#include "peripherals/at_timers.h"
#include "sidewalk/at_uplink.h"
#include "sidewalk/at_downlink.h"

#ifdef ALMANAC_UPDATE
#include "lr1110/almanac_update.h"
#endif /* ALMANAC_UPDATE */


#include "asset_tracker_version.h"
#include <sidewalk_version.h>

static struct k_thread at_thread;
K_THREAD_STACK_DEFINE(at_thread_stack, CONFIG_SIDEWALK_THREAD_STACK_SIZE);
K_MSGQ_DEFINE(at_thread_msgq, sizeof(at_event_t), CONFIG_SIDEWALK_THREAD_QUEUE_SIZE, 4);

// static struct k_thread at_receive_task;
// K_THREAD_STACK_DEFINE(at_receive_task_stack, RECEIVE_TASK_STACK_SIZE);
// K_MSGQ_DEFINE(at_rx_task_msgq, sizeof(struct at_rx_msg), CONFIG_SIDEWALK_THREAD_QUEUE_SIZE, 4);

#if defined(CONFIG_SIDEWALK_CLI)
	void app_event_send_standby() { }
	void app_event_send_sleep() { }
	void app_event_send_wake() { }
#endif

static at_ctx_t asset_tracker_context = {0};

void app_event_send_sid_init()
{
	// at_event_send(EVENT_SID_INIT);
}

static void at_app_entry(void *ctx, void *unused, void *unused2)
{
	at_ctx_t *at_ctx = (at_ctx_t *)ctx;
	ARG_UNUSED(unused);
	ARG_UNUSED(unused2);
	LOG_DBG("Starting %s ...", __FUNCTION__);
	
	PRINT_AWSIOT_LOGO();
	PRINT_AT_VERSION();

	if (application_pal_init()) {
		LOG_ERR("Failed to initialze PAL layer for sidewalk applicaiton.");
		return;
	}

	// PRINT_SIDEWALK_VERSION();

	sid_error_t err = sid_init(&at_ctx->sidewalk_config, &at_ctx->handle);

	switch (err) {
	case SID_ERROR_NONE:
		break;
	case SID_ERROR_ALREADY_INITIALIZED:
		LOG_WRN("Sidewalk already initialized!");
		break;
	default:
		LOG_ERR("Unknown error (%d) during sidewalk initialization!", err);
		return;
	}

	k_msleep(500); //let the LR11XX wakeup

	lr11xx_system_version_t version_trx = { 0x00 };
	// lr11xx_system_uid_t     uid         = { 0x00 };
	void *drv_ctx;
	drv_ctx = lr11xx_get_drv_ctx();
	if (lr11xx_system_wakeup(drv_ctx) != LR11XX_STATUS_OK) {
		LOG_ERR("scan_wifi wake-up fail");
		// return -1;
	}
	k_msleep(100); //let the LR11XX wakeup
	lr11xx_system_get_version( drv_ctx, &version_trx );
	PRINT_LR_VERSION();

#ifdef ALMANAC_UPDATE
	almanac_update();
#endif /* ALMANAC_UPDATE */

	err = sid_start(at_ctx->handle, at_ctx->sidewalk_config.link_mask);
	if (err) {
		LOG_ERR("Unknown error (%d) during sidewalk start!", err);
		return;
	}
#if defined(CONFIG_SIDEWALK_CLI)
	CLI_init(at_ctx->handle, &at_ctx->sidewalk_config);
#endif
#if defined(CONFIG_LR1110_CLI)
	LR1110_CLI_init(at_ctx->handle);
#endif

	at_ctx->sidewalk_state = STATE_SIDEWALK_NOT_READY;
	while (true) {
		at_event_t event = SIDEWALK_EVENT;
		loc_scan_t scan_type = at_ctx->at_conf.loc_scan_mode;

		if (!k_msgq_get(&at_thread_msgq, &event, K_FOREVER)) {
			switch (event) {
				case SIDEWALK_EVENT:
					err = sid_process(at_ctx->handle);
					if (err) {
						LOG_DBG("sid_process returned %d", err);
					}
					break;
				
				case BUTTON_EVENT_SHORT:
					if(at_ctx->total_msg == 0){
						LOG_INF("Immediate scan and uplink triggered...");
						scan_timer_set_and_run(K_MSEC(2000));
					} else {
						LOG_INF("Uplink in progress. Try again later!");
					}
					break;
				
				case BUTTON_EVENT_LONG:
					LOG_INF("Switching workshop mode...");
					at_event_send(EVENT_SWITCH_WS_MODE);
					break;
				
				case EVENT_SWITCH_WS_MODE:
					if(at_ctx->at_conf.workshop_mode==true) {
						at_ctx->at_conf.sid_link_type = LORA_LM;
						at_ctx->at_conf.loc_scan_mode = (loc_scan_t) GNSS_WIFI;
						at_ctx->at_conf.workshop_mode = false;
						LOG_INF("Workshop mode disabled.");
						at_event_send(EVENT_SID_STOP);
						scan_timer_set_and_run(K_MSEC(2000));

					} else {
						at_ctx->at_conf.sid_link_type = BLE_LM;
						at_ctx->at_conf.loc_scan_mode = (loc_scan_t) WIFI;
						at_ctx->at_conf.workshop_mode = true;
						LOG_INF("Workshop mode enabled.");
						at_event_send(EVENT_SID_STOP);
						scan_timer_set_and_run(K_MSEC(2000));
					}
					LOG_INF("Restarting stack with new settings...");
					break;

				case EVENT_RADIO_SWITCH:
					LOG_INF("Switching Sidewalk radios...");
					at_event_send(EVENT_SID_STOP);
					scan_timer_set_and_run(K_MSEC(2000));
					break;

				case EVENT_BLE_CONNECTION_REQUEST:
					LOG_INF("Requesting BLE connection...");
					err = sid_ble_bcn_connection_request(at_ctx->handle, true);
					if ( err != SID_ERROR_NONE ) {
						LOG_ERR("Error setting BLE connection request");
					}
					ble_conn_timer_set_and_run();
					at_event_send(EVENT_BLE_CONNECTION_WAIT);
					break;

				case EVENT_BLE_CONNECTION_WAIT:
					if(at_ctx->sidewalk_state == STATE_SIDEWALK_READY && (at_ctx->link_status.link_status_mask & BLE_LM) == LINK_UP) {
						at_event_send(EVENT_SEND_UPLINK);
						ble_conn_timer_stop();
					} else {
						if (ble_timeout == true) {
							ble_conn_timer_stop();
							at_event_send(EVENT_SID_STOP);
						} else {
							at_event_send(EVENT_BLE_CONNECTION_WAIT);
							k_msleep(20); //sleep to give way to other threads in this state
						}
					}
					break;

				case EVENT_SEND_UPLINK:
					if(at_ctx->at_conf.sid_link_type == BLE_LM && (at_ctx->sidewalk_state != STATE_SIDEWALK_READY || (at_ctx->link_status.link_status_mask & BLE_LM) == LINK_DOWN)) {
						at_event_send(EVENT_BLE_CONNECTION_REQUEST);			
					} else {
						LOG_INF("Sending uplink...");
						at_send_uplink(at_ctx);
					}
					break;

				case EVENT_UPLINK_COMPLETE:
					LOG_INF("Uplink complete...");
					
					at_event_send(EVENT_SID_STOP);
					break;

				case EVENT_SCAN_SENSORS:
					LOG_INF("Scanning sensors...");
					get_temp_hum(&at_ctx->sensors);
					get_accel(&at_ctx->sensors);
					get_batt(&at_ctx->sensors);
					break;

				case EVENT_SCAN_LOC:
					LOG_INF("Scanning location...");
					switch(scan_type) {
						case WIFI:
							scan_wifi(at_ctx);
							break;
						case GNSS:
						case GNSS_WIFI:
							start_gnss_scan(at_ctx);
							break;
						default:
							LOG_ERR("Invalid location scan type");
					}
					break;

				case EVENT_SID_STOP:
					LOG_INF("Going to sleep...");
					err = sid_process(at_ctx->handle);
					if (err) {
						LOG_DBG("sid_process returned %d", err);
					}
					err = sid_stop(at_ctx->handle, at_ctx->sidewalk_config.link_mask);
					if (err) {
						LOG_ERR("sid_stop returned %d", err); 
					}
					k_msgq_purge(&at_thread_msgq);
					break;

				case EVENT_SID_START:
					LOG_INF("Starting Sidewalk stack...");
					err = sid_start(at_ctx->handle, at_ctx->at_conf.sid_link_type);
					if (err) {
						LOG_ERR("sid_start returned %d", err);
					}
					break;

				default:
					LOG_ERR("Invalid Event received!");
			}
		}
	}
}

void at_event_send(at_event_t event)
{
	int ret = k_msgq_put(&at_thread_msgq, (void *)&event,
			     k_is_in_isr() ? K_NO_WAIT : K_FOREVER);

	if (ret) {
		LOG_ERR("Failed to send event to asset tracker thread. err: %d", ret);
	}
}

// void at_rx_task_msg_q_write(struct at_rx_msg *rx_msg)
// {
// 	while (k_msgq_put(&at_rx_task_msgq, rx_msg, K_NO_WAIT)) {
// 		LOG_WRN("The at_rx_task_msgq queue is full, purge old data");
// 		k_msgq_purge(&at_rx_task_msgq);
// 	}
// }

sid_error_t at_thread_init(void)
{
	//load config
	asset_tracker_context.at_conf = (struct at_config) {
		.max_rec = 100,
		// .sid_link_type = LORA_LM,
		.sid_link_type = BLE_LM,
		.loc_scan_mode = (loc_scan_t) WIFI,
		.gnss_scan_mode = (gnss_scan_mode_t) GNSS_AUTONOMOUS,
		.motion_period = CONFIG_IN_MOTION_PER_M,
		.scan_freq_motion = CONFIG_MOTION_SCAN_PER_S,
		.motion_thres = 5,
		.scan_freq_static = CONFIG_STATIC_SCAN_PER_M,
		.workshop_mode = true,
		.workshop_wifi_macs = {
			STATIC_WS_MAC1,
			STATIC_WS_MAC2
		},
		.workshop_wifi_rssi = {
			STATIC_WS_RSSI1,
			STATIC_WS_RSSI2
		}
	};
	// LOG_INF("Loading config... DONE!");
	asset_tracker_context.sidewalk_config = (struct sid_config) {
		.link_mask = (BLE_LM | LORA_LM),
		.time_sync_periodicity_seconds = 86400,
		.callbacks = &asset_tracker_context.event_callbacks,
		.link_config = app_get_ble_config(),
		.sub_ghz_link_config = app_get_sub_ghz_config(),
	};

	asset_tracker_context.sidewalk_state = STATE_SIDEWALK_INIT;

	if (sidewalk_callbacks_set(&asset_tracker_context, &asset_tracker_context.event_callbacks)) {
		LOG_ERR("Failed to set sidewalk callbacks");
		SID_PAL_ASSERT(false);
	}

	#if defined(CONFIG_ASSET_TRACKER_CLI)
	AT_CLI_init(&asset_tracker_context);
	#endif

	(void)k_thread_create(&at_thread, at_thread_stack,
			      K_THREAD_STACK_SIZEOF(at_thread_stack), at_app_entry,
			      &asset_tracker_context, NULL, NULL, CONFIG_SIDEWALK_THREAD_PRIORITY, 0, K_NO_WAIT);

	// (void)k_thread_create(&at_receive_task, at_receive_task_stack,
	// 		      K_THREAD_STACK_SIZEOF(at_receive_task_stack), at_rx_task,
	// 		      &asset_tracker_context, NULL, NULL, RECEIVE_TASK_PRIORITY, 0, K_NO_WAIT);

	k_thread_name_set(&at_thread, "sidewalk_at_thread");
	// k_thread_name_set(&at_receive_task, "at_receive_task");
	return SID_ERROR_NONE;
}

// called by lr1110_shell
void app_event_wifi_scan()
{
	at_event_send(EVENT_SCAN_LOC);
}

