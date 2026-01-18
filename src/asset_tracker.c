// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <sid_api.h>
#include <sid_error.h>
#include <sid_location.h>
#include <zephyr/kernel.h>

// New SDK v1.19 platform init
#include <sid_pal_common_ifc.h>
#include <sid_pal_radio_ifc.h>
#include <app_mfg_config.h>

#if defined(CONFIG_ASSET_TRACKER_CLI)
#include "at_shell.h"
#include "location_shell.h"
#endif

#include <halo_lr11xx_radio.h>
#include <zephyr/drivers/gpio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(asset_tracker, CONFIG_TRACKER_LOG_LEVEL);

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <bt_app_callbacks.h>

#include "sidewalk/sidewalk_callbacks.h"
#include <sid_pal_assert_ifc.h>
#include <app_ble_config.h>
#include <app_subGHz_config.h>

#ifdef CONFIG_SIDEWALK_SUBGHZ_RADIO_LR1110
#include <app_location_lr11xx_config.h>
#endif

#include <asset_tracker.h>
#include "peripherals/at_battery.h"
#include "peripherals/at_lis3dh.h"
#include "peripherals/at_sht41.h"
#include "peripherals/at_timers.h"
#include "sidewalk/at_uplink.h"
#include "sidewalk/at_downlink.h"

#include "asset_tracker_version.h"
#include <sidewalk_version.h>

static struct k_thread at_thread;
K_THREAD_STACK_DEFINE(at_thread_stack, CONFIG_SIDEWALK_THREAD_STACK_SIZE);
K_MSGQ_DEFINE(at_thread_msgq, sizeof(at_event_t), CONFIG_SIDEWALK_THREAD_QUEUE_SIZE, 4);

static at_ctx_t asset_tracker_context = {0};

/**
 * Pre-configure LR1110 GPIO pins as INPUT before SDK registration
 * This ensures the pins are readable when the SDK's wait_on_busy is called
 */
static int preconfigure_lr1110_gpios(void)
{
	/* LR1110 busy pin: P1.11 */
	const struct gpio_dt_spec busy_gpio = {
		.port = DEVICE_DT_GET(DT_NODELABEL(gpio1)),
		.pin = 11,
		.dt_flags = GPIO_ACTIVE_HIGH,
	};
	
	/* LR1110 event/DIO1 pin: P0.02 */
	const struct gpio_dt_spec event_gpio = {
		.port = DEVICE_DT_GET(DT_NODELABEL(gpio0)),
		.pin = 2,
		.dt_flags = GPIO_ACTIVE_HIGH,
	};
	
	if (!device_is_ready(busy_gpio.port)) {
		LOG_ERR("GPIO1 port not ready");
		return -ENODEV;
	}
	
	if (!device_is_ready(event_gpio.port)) {
		LOG_ERR("GPIO0 port not ready");
		return -ENODEV;
	}
	
	int ret = gpio_pin_configure(busy_gpio.port, busy_gpio.pin, GPIO_INPUT);
	if (ret < 0) {
		LOG_ERR("Failed to configure busy GPIO as input: %d", ret);
		return ret;
	}
	
	ret = gpio_pin_configure(event_gpio.port, event_gpio.pin, GPIO_INPUT | GPIO_PULL_DOWN);
	if (ret < 0) {
		LOG_ERR("Failed to configure event GPIO as input: %d", ret);
		return ret;
	}
	
	LOG_INF("LR1110 GPIOs pre-configured as INPUT");
	return 0;
}

#ifdef CONFIG_SIDEWALK_SUBGHZ_SUPPORT
static sid_pal_radio_rx_packet_t radio_rx_packet;

static void radio_event_notifier(sid_pal_radio_events_t event)
{
	LOG_DBG("Radio event %d", event);
}

static void radio_irq_handler(void)
{
	LOG_DBG("Radio IRQ");
}
#endif /* CONFIG_SIDEWALK_SUBGHZ_SUPPORT */

/**
 * Location callback - called when location scan/send completes
 */
static void location_callback(const struct sid_location_result *const result, void *context)
{
	ARG_UNUSED(context);
	
	LOG_INF("Location result: status=%d, err=%d, mode=%d, link=%d", 
		result->status, result->err, result->mode, result->link);
	
	if (result->status == SID_LOCATION_SCAN_DONE) {
		LOG_INF("Location scan complete");
	} else if (result->status == SID_LOCATION_SEND_DONE) {
		LOG_INF("Location send complete");
		// After location is sent, send sensor telemetry
		at_event_send(EVENT_SEND_UPLINK);
	}
	
	if (result->err != SID_ERROR_NONE) {
		LOG_ERR("Location error: %d", result->err);
		// Still send sensor telemetry even if location failed
		at_event_send(EVENT_SEND_UPLINK);
	}
}

/**
 * Initialize the sid_location API
 */
static sid_error_t init_location_services(at_ctx_t *at_ctx)
{
	struct sid_location_config loc_cfg = {
		.sid_location_type_mask = SID_LOCATION_METHOD_ALL,
		.max_effort = SID_LOCATION_EFFORT_L4,
		.manage_effort = true,
		.callbacks = {
			.on_update = location_callback,
			.context = at_ctx,
		},
		.stepdowns = {
			.l4_to_l3 = 5000,
			.l3_to_l2 = 5000,
			.l2_to_l1 = 5000,
		},
		.fragmentation = {
			.timeout_ms = 30000,
			.max_retries = 3,
		},
	};
	
	sid_error_t err = sid_location_init(at_ctx->handle, &loc_cfg);
	if (err != SID_ERROR_NONE) {
		LOG_ERR("Failed to initialize location services: %d", err);
	} else {
		LOG_INF("Location services initialized");
	}
	return err;
}

/**
 * Trigger a location scan and send
 */
static void trigger_location_scan(at_ctx_t *at_ctx)
{
	struct sid_location_run_config run_cfg = {
		.type = SID_LOCATION_SCAN_AND_SEND,
		.mode = SID_LOCATION_EFFORT_DEFAULT,
		.buffer = NULL,
		.size = 0,
	};
	
	sid_error_t err = sid_location_run(at_ctx->handle, &run_cfg, 0);
	if (err != SID_ERROR_NONE) {
		LOG_ERR("Failed to start location scan: %d", err);
		// Fall back to just sending sensor telemetry
		at_event_send(EVENT_SEND_UPLINK);
	} else {
		LOG_INF("Location scan started");
	}
}

static void at_app_entry(void *ctx, void *unused, void *unused2)
{
	at_ctx_t *at_ctx = (at_ctx_t *)ctx;
	ARG_UNUSED(unused);
	ARG_UNUSED(unused2);
	LOG_DBG("Starting %s ...", __FUNCTION__);
	
	PRINT_AWSIOT_LOGO();
	PRINT_AT_VERSION();

	// Pre-configure LR1110 GPIOs as INPUT before SDK registration
	int gpio_ret = preconfigure_lr1110_gpios();
	if (gpio_ret < 0) {
		LOG_ERR("Failed to pre-configure LR1110 GPIOs: %d", gpio_ret);
		// Continue anyway - the SDK might still work
	}

	// Initialize platform using new SDK v1.19 API
	platform_parameters_t platform_parameters = {
		.mfg_store_region.addr_start = APP_MFG_CFG_FLASH_START,
		.mfg_store_region.addr_end = APP_MFG_CFG_FLASH_END,
#ifdef CONFIG_SIDEWALK_SUBGHZ_SUPPORT
		.platform_init_parameters.radio_cfg = get_radio_cfg(),
#ifdef CONFIG_SIDEWALK_SUBGHZ_RADIO_LR1110
		.platform_init_parameters.gnss_wifi_cfg = (lr11xx_gnss_wifi_config_t *)get_location_cfg(),
#endif
#endif
	};

	sid_error_t err = sid_platform_init(&platform_parameters);
	if (err != SID_ERROR_NONE) {
		LOG_ERR("Failed to initialize Sidewalk platform: %d", err);
		return;
	}

	if (app_mfg_cfg_is_empty()) {
		LOG_ERR("The mfg.hex version mismatch");
		LOG_ERR("Check if the file has been generated and flashed properly");
		LOG_ERR("START ADDRESS: 0x%08x", APP_MFG_CFG_FLASH_START);
		LOG_ERR("SIZE: 0x%08x", APP_MFG_CFG_FLASH_SIZE);
		return;
	}

#ifdef CONFIG_SIDEWALK_SUBGHZ_SUPPORT
	// Always initialize radio at startup so we can switch to LoRa later
	LOG_INF("Initializing radio...");
	int32_t radio_err = sid_pal_radio_init(radio_event_notifier, radio_irq_handler, &radio_rx_packet);
	if (radio_err) {
		LOG_ERR("Radio init failed: %d", radio_err);
	} else {
		LOG_INF("Radio init success");
	}
	radio_err = sid_pal_radio_sleep(0);
	if (radio_err) {
		LOG_ERR("Radio sleep failed: %d", radio_err);
	}
#endif

	LOG_INF("Calling sid_init...");
	err = sid_init(&at_ctx->sidewalk_config, &at_ctx->handle);
	LOG_INF("sid_init returned: %d", err);

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

#ifdef CONFIG_SIDEWALK_SUBGHZ_SUPPORT
	// Always print LR11XX version since radio is always initialized
	k_msleep(500); // Let the LR11XX wake up
	LOG_INF("LR11XX sleep done, getting drv_ctx...");

	lr11xx_system_version_t version_trx = { 0x00 };
	void *drv_ctx;
	drv_ctx = lr11xx_get_drv_ctx();
	LOG_INF("Got drv_ctx, about to wake LR11XX...");
	if (lr11xx_system_wakeup(drv_ctx) != LR11XX_STATUS_OK) {
		LOG_ERR("LR11XX wake-up failed");
	}
	k_msleep(100);
	lr11xx_system_get_version(drv_ctx, &version_trx);
	PRINT_LR_VERSION();
#endif

	LOG_INF("Calling sid_start with default link type...");
	err = sid_start(at_ctx->handle, at_ctx->at_conf.sid_link_type);
	LOG_INF("sid_start returned: %d", err);
	if (err) {
		LOG_ERR("Unknown error (%d) during sidewalk start!", err);
		return;
	}
	at_ctx->stack_started = true;

	// Initialize location services
	init_location_services(at_ctx);

	#if defined(CONFIG_ASSET_TRACKER_CLI)
	AT_CLI_init(at_ctx);
	location_shell_init(at_ctx);
	#endif

	at_ctx->sidewalk_state = STATE_SIDEWALK_NOT_READY;
	
	while (true) {
		at_event_t event = SIDEWALK_EVENT;

		if (!k_msgq_get(&at_thread_msgq, &event, K_FOREVER)) {
			switch (event) {
			case SIDEWALK_EVENT:
				err = sid_process(at_ctx->handle);
				if (err) {
					LOG_DBG("sid_process returned %d", err);
				}
				break;
			
			case BUTTON_EVENT_SHORT:
				if (at_ctx->total_msg == 0) {
					LOG_INF("Immediate scan and uplink triggered...");
					scan_timer_set_and_run(K_MSEC(5000));
				} else {
					LOG_INF("Uplink in progress. Try again later!");
				}
				break;
			
			case BUTTON_EVENT_LONG:
				LOG_INF("Long button press - toggling link type...");
				at_event_send(EVENT_RADIO_SWITCH);
				break;

			case EVENT_RADIO_SWITCH:
				LOG_INF("Switching Sidewalk radio to %s...", 
					(at_ctx->at_conf.sid_link_type == BLE_LM) ? "BLE" : "LoRa");
				at_event_send(EVENT_SID_STOP);
				scan_timer_set_and_run(K_MSEC(2000));
				break;

			case EVENT_BLE_CONNECTION_REQUEST:
				LOG_INF("Requesting BLE connection...");
				err = sid_ble_bcn_connection_request(at_ctx->handle, true);
				if (err != SID_ERROR_NONE) {
					LOG_ERR("Error setting BLE connection request");
				}
				ble_conn_timer_set_and_run();
				at_event_send(EVENT_BLE_CONNECTION_WAIT);
				break;

			case EVENT_BLE_CONNECTION_WAIT:
				if (at_ctx->sidewalk_state == STATE_SIDEWALK_READY && 
				    (at_ctx->link_status.link_status_mask & BLE_LM) != 0) {
					at_event_send(EVENT_SEND_UPLINK);
					ble_conn_timer_stop();
				} else {
					if (ble_timeout == true) {
						ble_conn_timer_stop();
						at_event_send(EVENT_SID_STOP);
					} else {
						at_event_send(EVENT_BLE_CONNECTION_WAIT);
						k_msleep(20);
					}
				}
				break;

			case EVENT_SEND_UPLINK:
				// For BLE, we need to request connection first if link is down
				if (at_ctx->at_conf.sid_link_type == BLE_LM) {
					if (at_ctx->sidewalk_state != STATE_SIDEWALK_READY || 
					    (at_ctx->link_status.link_status_mask & BLE_LM) == 0) {
						at_event_send(EVENT_BLE_CONNECTION_REQUEST);
						break;
					}
				}
				// For LoRa, just need time sync - it's connectionless/fire-and-forget
				// No need to check link_status_mask for LoRa
				LOG_INF("Sending uplink...");
				at_send_uplink(at_ctx);
				break;

			case EVENT_UPLINK_COMPLETE:
				LOG_INF("Uplink complete.");
				// Stack stays running - no longer stopping after each uplink
				break;

			case EVENT_SCAN_SENSORS:
				LOG_INF("Scanning sensors...");
				get_temp_hum(&at_ctx->sensors);
				get_accel(&at_ctx->sensors);
				get_batt(&at_ctx->sensors);
				break;

			case EVENT_SCAN_LOC:
				LOG_INF("Triggering location scan via SDK...");
				trigger_location_scan(at_ctx);
				break;

			case EVENT_SID_STOP:
				LOG_INF("Going to sleep...");
				if (at_ctx->stack_started) {
					err = sid_process(at_ctx->handle);
					if (err) {
						LOG_DBG("sid_process returned %d", err);
					}
					LOG_INF("Calling sid_stop with link_type 0x%x", 
						at_ctx->at_conf.sid_link_type);
					err = sid_stop(at_ctx->handle, at_ctx->at_conf.sid_link_type);
					LOG_INF("sid_stop returned %d", err);
					at_ctx->stack_started = false;
				} else {
					LOG_DBG("Stack not running, skipping sid_stop");
				}
				k_msgq_purge(&at_thread_msgq);
				break;

			case EVENT_SID_START:
				LOG_INF("EVENT_SID_START: stack_started=%d", at_ctx->stack_started);
				if (at_ctx->stack_started) {
					LOG_DBG("Sidewalk stack already started, skipping sid_start");
				} else {
					LOG_INF("Starting Sidewalk stack with link_type 0x%x...", 
						at_ctx->at_conf.sid_link_type);
					err = sid_start(at_ctx->handle, at_ctx->at_conf.sid_link_type);
					if (err) {
						LOG_ERR("sid_start returned %d", err);
					} else {
						at_ctx->stack_started = true;
						LOG_INF("stack_started set to true");
						// Re-initialize location services after stack restart
						sid_location_deinit(at_ctx->handle);
						init_location_services(at_ctx);
					}
				}
				break;

			case EVENT_BLE_LOCATION_START:
				/* Switch to BLE-only mode for L1 location */
				LOG_INF("Switching to BLE-only mode for L1 location...");
				
				/* Set pending flag - will trigger location when BLE is ready */
				at_ctx->ble_location_pending = true;
				
				/* Stop current stack */
				if (at_ctx->stack_started) {
					sid_location_deinit(at_ctx->handle);
					err = sid_stop(at_ctx->handle, at_ctx->sidewalk_config.link_mask);
					LOG_INF("sid_stop returned %d", err);
					at_ctx->stack_started = false;
				}
				
				/* Deinit and reinit with BLE-only */
				sid_deinit(at_ctx->handle);
				at_ctx->handle = NULL;
				
				/* Reinit with BLE-only config */
				struct sid_config ble_only_config = at_ctx->sidewalk_config;
				ble_only_config.link_mask = BLE_LM;
				ble_only_config.sub_ghz_link_config = NULL;
				
				err = sid_init(&ble_only_config, &at_ctx->handle);
				if (err != SID_ERROR_NONE) {
					LOG_ERR("sid_init (BLE-only) failed: %d", err);
					at_ctx->ble_location_pending = false;
					at_event_send(EVENT_RESTORE_FULL_STACK);
					break;
				}
				
				err = sid_start(at_ctx->handle, BLE_LM);
				if (err != SID_ERROR_NONE) {
					LOG_ERR("sid_start (BLE-only) failed: %d", err);
					at_ctx->ble_location_pending = false;
					at_event_send(EVENT_RESTORE_FULL_STACK);
					break;
				}
				at_ctx->stack_started = true;
				LOG_INF("BLE-only stack started, requesting connection...");
				
				/* Request BLE connection - location will trigger when ready */
				err = sid_ble_bcn_connection_request(at_ctx->handle, true);
				if (err != SID_ERROR_NONE) {
					LOG_ERR("Error setting BLE connection request: %d", err);
				}
				break;

			case EVENT_BLE_LOCATION_READY:
				/* BLE stack is ready, now trigger L1 location */
				LOG_INF("BLE ready, initializing and running L1 location...");
				at_ctx->ble_location_pending = false;
				
				/* Trigger the BLE location from location_shell */
				location_shell_trigger_ble_location();
				break;

			case EVENT_RESTORE_FULL_STACK:
				/* Restore full stack after BLE location */
				LOG_INF("Restoring full stack (BLE + LoRa)...");
				
				/* Stop and deinit current stack */
				if (at_ctx->stack_started) {
					sid_location_deinit(at_ctx->handle);
					sid_stop(at_ctx->handle, BLE_LM);
					at_ctx->stack_started = false;
				}
				if (at_ctx->handle) {
					sid_deinit(at_ctx->handle);
					at_ctx->handle = NULL;
				}
				
				/* Reinit with full config */
				err = sid_init(&at_ctx->sidewalk_config, &at_ctx->handle);
				if (err != SID_ERROR_NONE) {
					LOG_ERR("sid_init (full) failed: %d", err);
					break;
				}
				
				/* Start with configured link type */
				err = sid_start(at_ctx->handle, at_ctx->at_conf.sid_link_type);
				if (err != SID_ERROR_NONE) {
					LOG_ERR("sid_start (full) failed: %d", err);
					break;
				}
				at_ctx->stack_started = true;
				
				/* Reinit location services with full config */
				init_location_services(at_ctx);
				
				LOG_INF("Full stack restored, link_type=0x%x", at_ctx->at_conf.sid_link_type);
				break;

			default:
				LOG_ERR("Invalid Event received: %d", event);
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

/**
 * GATT authorization callback - filters BLE attributes based on connection ID
 * This is required for proper Sidewalk BLE operation
 */
static bool gatt_authorize(struct bt_conn *conn, const struct bt_gatt_attr *attr)
{
	struct bt_conn_info cinfo = {};
	int ret = bt_conn_get_info(conn, &cinfo);
	if (ret != 0) {
		LOG_ERR("Failed to get id of connection err %d", ret);
		return false;
	}

	if (cinfo.id == BT_ID_SIDEWALK) {
		if (sid_ble_bt_attr_is_SMP(attr)) {
			return false;
		}
	}

	return true;
}

static const struct bt_gatt_authorization_cb gatt_authorization_callbacks = {
	.read_authorize = gatt_authorize,
	.write_authorize = gatt_authorize,
};

sid_error_t at_thread_init(void)
{
	// Load config - default to LoRa for asset tracking
	asset_tracker_context.at_conf = (struct at_config) {
		.max_rec = 100,
		.sid_link_type = LORA_LM,
		.motion_period = CONFIG_IN_MOTION_PER_M,
		.scan_freq_motion = CONFIG_MOTION_SCAN_PER_S,
		.motion_thres = 5,
		.scan_freq_static = CONFIG_STATIC_SCAN_PER_M,
	};

	asset_tracker_context.sidewalk_config = (struct sid_config) {
		.link_mask = (BLE_LM | LORA_LM),  // Init with all supported links, start with default
		.dev_ch = {
			.type = SID_END_DEVICE_TYPE_STATIC,
			.power_type = SID_END_DEVICE_POWERED_BY_BATTERY_AND_LINE_POWER,
			.qualification_id = 0x0001,
		},
		.callbacks = &asset_tracker_context.event_callbacks,
		.link_config = app_get_ble_config(),
		.sub_ghz_link_config = app_get_sub_ghz_config(),
		.log_config = NULL,
		.time_sync_config = NULL,
	};

	asset_tracker_context.sidewalk_state = STATE_SIDEWALK_INIT;

	if (sidewalk_callbacks_set(&asset_tracker_context, &asset_tracker_context.event_callbacks)) {
		LOG_ERR("Failed to set sidewalk callbacks");
		SID_PAL_ASSERT(false);
	}

	#if defined(CONFIG_ASSET_TRACKER_CLI)
	AT_CLI_init(&asset_tracker_context);
	#endif

	// Register GATT authorization callbacks before starting BLE
	int err = bt_gatt_authorization_cb_register(&gatt_authorization_callbacks);
	if (err) {
		LOG_ERR("Registering GATT authorization callbacks failed (err %d)", err);
		return SID_ERROR_GENERIC;
	}
	LOG_INF("GATT authorization callbacks registered");

	(void)k_thread_create(&at_thread, at_thread_stack,
			      K_THREAD_STACK_SIZEOF(at_thread_stack), at_app_entry,
			      &asset_tracker_context, NULL, NULL, CONFIG_SIDEWALK_THREAD_PRIORITY, 0, K_NO_WAIT);

	k_thread_name_set(&at_thread, "sidewalk_at_thread");
	return SID_ERROR_NONE;
}

// Called by timer to trigger location scan
void app_event_wifi_scan()
{
	at_event_send(EVENT_SCAN_LOC);
}
