/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: MIT-0
 * 
 * Location shell commands for WM1110 Asset Tracker
 * Adapted from Nordic sid_end_device sample
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/shell/shell.h>
#include <zephyr/sys/util.h>

#include <sid_api.h>
#include <sid_location.h>
#include <sid_pal_radio_ifc.h>

#include <location_shell.h>
#include <asset_tracker.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(location_shell, CONFIG_TRACKER_LOG_LEVEL);

static at_ctx_t *loc_ctx = NULL;
static bool location_initialized = true;  // Already initialized by asset_tracker at startup
static const struct shell *pending_shell = NULL;
static bool ble_location_pending = false;
static bool ble_location_initialized = false;  // Track if location lib is initialized for BLE mode

/* Location callback */
static void location_shell_callback(const struct sid_location_result *const result, void *context)
{
	ARG_UNUSED(context);
	
	LOG_INF("Location result: status=%d, err=%d, mode=%d, link=%d", 
		result->status, result->err, result->mode, result->link);
	
	const char *mode_str = "unknown";
	switch (result->mode) {
	case SID_LOCATION_EFFORT_L1: mode_str = "L1 (BLE)"; break;
	case SID_LOCATION_EFFORT_L2: mode_str = "L2 (LoRa)"; break;
	case SID_LOCATION_EFFORT_L3: mode_str = "L3 (WiFi)"; break;
	case SID_LOCATION_EFFORT_L4: mode_str = "L4 (GNSS)"; break;
	default: break;
	}
	
	if (result->status == SID_LOCATION_SCAN_DONE) {
		LOG_INF("Location scan complete - mode: %s", mode_str);
		if (result->mode == SID_LOCATION_EFFORT_L3 || result->mode == SID_LOCATION_EFFORT_L4) {
			LOG_INF("Payload size: %d bytes", result->size);
			if (result->size > 0) {
				LOG_HEXDUMP_INF(result->payload, result->size, "Location payload");
			}
		}
	} else if (result->status == SID_LOCATION_SEND_DONE) {
		LOG_INF("Location send complete - mode: %s", mode_str);
		
		/* If this was a BLE location (L1), restore the full stack */
		if (ble_location_pending && result->mode == SID_LOCATION_EFFORT_L1) {
			LOG_INF("BLE location complete, restoring full stack...");
			ble_location_pending = false;
			ble_location_initialized = false;
			/* Send event to restore full stack */
			at_event_send(EVENT_RESTORE_FULL_STACK);
		}
	}
	
	if (result->err != SID_ERROR_NONE) {
		LOG_ERR("Location error: %d", result->err);
		/* On error, also restore if we were doing BLE location */
		if (ble_location_pending) {
			ble_location_pending = false;
			ble_location_initialized = false;
			at_event_send(EVENT_RESTORE_FULL_STACK);
		}
	}
}

/* Location Subcommands */
SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_location,
	SHELL_CMD_ARG(init, NULL, CMD_LOCATION_INIT_DESCRIPTION, cmd_location_init,
		      CMD_LOCATION_INIT_ARG_REQUIRED, CMD_LOCATION_INIT_ARG_OPTIONAL),
	SHELL_CMD_ARG(deinit, NULL, CMD_LOCATION_DEINIT_DESCRIPTION, cmd_location_deinit,
		      CMD_LOCATION_DEINIT_ARG_REQUIRED, CMD_LOCATION_DEINIT_ARG_OPTIONAL),
	SHELL_CMD_ARG(send, NULL, CMD_LOCATION_SEND_DESCRIPTION, cmd_location_send,
		      CMD_LOCATION_SEND_ARG_REQUIRED, CMD_LOCATION_SEND_ARG_OPTIONAL),
	SHELL_CMD_ARG(scan, NULL, CMD_LOCATION_SCAN_DESCRIPTION, cmd_location_scan,
		      CMD_LOCATION_SCAN_ARG_REQUIRED, CMD_LOCATION_SCAN_ARG_OPTIONAL),
	SHELL_CMD_ARG(status, NULL, CMD_LOCATION_STATUS_DESCRIPTION, cmd_location_status,
		      CMD_LOCATION_STATUS_ARG_REQUIRED, CMD_LOCATION_STATUS_ARG_OPTIONAL),
	SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(location, &sub_location, "Sidewalk Location CLI", NULL);

void location_shell_init(at_ctx_t *ctx)
{
	loc_ctx = ctx;
	LOG_INF("Location shell initialized");
}

int cmd_location_init(const struct shell *shell, int32_t argc, const char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!loc_ctx || !loc_ctx->handle) {
		shell_error(shell, "Sidewalk not initialized");
		return -EINVAL;
	}

	if (location_initialized) {
		shell_warn(shell, "Location already initialized");
		return 0;
	}

	struct sid_location_config cfg = {
#ifdef CONFIG_SIDEWALK_SUBGHZ_RADIO_LR1110
		.sid_location_type_mask = SID_LOCATION_METHOD_ALL,
		.max_effort = SID_LOCATION_EFFORT_L4,
#else
		.sid_location_type_mask = SID_LOCATION_METHOD_BLE_GATEWAY,
		.max_effort = SID_LOCATION_EFFORT_L1,
#endif
		.manage_effort = true,
		.callbacks = {
			.on_update = location_shell_callback,
			.context = loc_ctx,
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

	sid_error_t err = sid_location_init(loc_ctx->handle, &cfg);
	if (err != SID_ERROR_NONE) {
		shell_error(shell, "sid_location_init failed: %d", err);
		return -EIO;
	}

	location_initialized = true;
	shell_print(shell, "Location services initialized");
	return 0;
}

int cmd_location_deinit(const struct shell *shell, int32_t argc, const char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	if (!loc_ctx || !loc_ctx->handle) {
		shell_error(shell, "Sidewalk not initialized");
		return -EINVAL;
	}

	if (!location_initialized) {
		shell_warn(shell, "Location not initialized");
		return 0;
	}

	sid_error_t err = sid_location_deinit(loc_ctx->handle);
	if (err != SID_ERROR_NONE) {
		shell_error(shell, "sid_location_deinit failed: %d", err);
		return -EIO;
	}

	location_initialized = false;
	shell_print(shell, "Location services deinitialized");
	return 0;
}

int cmd_location_send(const struct shell *shell, int32_t argc, const char **argv)
{
	if (!loc_ctx || !loc_ctx->handle) {
		shell_error(shell, "Sidewalk not initialized");
		return -EINVAL;
	}

	enum sid_location_effort_mode mode = SID_LOCATION_EFFORT_DEFAULT;

	if (argc == 2) {
		char *end = NULL;
		long level = strtol(argv[1], &end, 0);
		if (end == argv[1] || !IN_RANGE(level, 1, 4)) {
			shell_error(shell, "Invalid level [%s], must be 1-4", argv[1]);
			return -EINVAL;
		}
		if (level == 2) {
			shell_warn(shell, "L2 (LoRa triangulation) is not supported");
			return -EINVAL;
		}
		mode = (enum sid_location_effort_mode)level;
	}

	/* Special handling for L1 (BLE location) */
	if (mode == SID_LOCATION_EFFORT_L1) {
		shell_print(shell, "BLE location requested - switching to BLE-only mode...");
		pending_shell = shell;
		ble_location_pending = true;
		/* Send event to switch to BLE-only and trigger L1 location */
		at_event_send(EVENT_BLE_LOCATION_START);
		return 0;
	}

	if (!location_initialized) {
		shell_error(shell, "Location not initialized. Run 'location init' first.");
		return -EINVAL;
	}

	struct sid_location_run_config run_cfg = {
		.type = SID_LOCATION_SCAN_AND_SEND,
		.mode = mode,
		.buffer = NULL,
		.size = 0,
	};

	sid_error_t err = sid_location_run(loc_ctx->handle, &run_cfg, 0);
	if (err != SID_ERROR_NONE) {
		shell_error(shell, "sid_location_run failed: %d", err);
		return -EIO;
	}

	shell_print(shell, "Location scan+send started (mode=%d)", mode);
	return 0;
}

int cmd_location_scan(const struct shell *shell, int32_t argc, const char **argv)
{
	if (!loc_ctx || !loc_ctx->handle) {
		shell_error(shell, "Sidewalk not initialized");
		return -EINVAL;
	}

	if (!location_initialized) {
		shell_error(shell, "Location not initialized. Run 'location init' first.");
		return -EINVAL;
	}

	enum sid_location_effort_mode mode = SID_LOCATION_EFFORT_DEFAULT;

	if (argc == 2) {
		char *end = NULL;
		long level = strtol(argv[1], &end, 0);
		if (end == argv[1] || !IN_RANGE(level, 1, 4)) {
			shell_error(shell, "Invalid level [%s], must be 1-4", argv[1]);
			return -EINVAL;
		}
		if (level == 1) {
			shell_warn(shell, "L1 (BLE) has no scan - use 'location send 1' instead");
			return -EINVAL;
		}
		if (level == 2) {
			shell_warn(shell, "L2 (LoRa triangulation) is not supported");
			return -EINVAL;
		}
		mode = (enum sid_location_effort_mode)level;
	}

	struct sid_location_run_config run_cfg = {
		.type = SID_LOCATION_SCAN_ONLY,
		.mode = mode,
		.buffer = NULL,
		.size = 0,
	};

	sid_error_t err = sid_location_run(loc_ctx->handle, &run_cfg, 0);
	if (err != SID_ERROR_NONE) {
		shell_error(shell, "sid_location_run failed: %d", err);
		return -EIO;
	}

	shell_print(shell, "Location scan started (mode=%d)", mode);
	return 0;
}

int cmd_location_status(const struct shell *shell, int32_t argc, const char **argv)
{
	ARG_UNUSED(argc);
	ARG_UNUSED(argv);

	shell_print(shell, "Location Status:");
	shell_print(shell, "  Initialized: %s", location_initialized ? "yes" : "no");
	shell_print(shell, "  BLE location pending: %s", ble_location_pending ? "yes" : "no");
	
	if (loc_ctx) {
		shell_print(shell, "  Sidewalk state: %d", loc_ctx->sidewalk_state);
		shell_print(shell, "  Link type: %s", 
			(loc_ctx->at_conf.sid_link_type == BLE_LM) ? "BLE" : 
			(loc_ctx->at_conf.sid_link_type == LORA_LM) ? "LoRa" : "Unknown");
		shell_print(shell, "  Stack started: %s", loc_ctx->stack_started ? "yes" : "no");
	} else {
		shell_print(shell, "  Context: not set");
	}

	return 0;
}

/* Called from asset_tracker when BLE-only stack is ready for L1 location */
void location_shell_trigger_ble_location(void)
{
	if (!loc_ctx || !loc_ctx->handle) {
		LOG_ERR("Cannot trigger BLE location - no context");
		ble_location_pending = false;
		ble_location_initialized = false;
		return;
	}

	/* Check if stack is ready and BLE link is up */
	if (loc_ctx->sidewalk_state != STATE_SIDEWALK_READY) {
		LOG_ERR("Cannot trigger BLE location - stack not ready (state=%d)", 
			loc_ctx->sidewalk_state);
		ble_location_pending = false;
		ble_location_initialized = false;
		at_event_send(EVENT_RESTORE_FULL_STACK);
		return;
	}

	if ((loc_ctx->link_status.link_status_mask & SID_LINK_TYPE_1) == 0) {
		LOG_ERR("Cannot trigger BLE location - BLE link not up (mask=0x%x)",
			loc_ctx->link_status.link_status_mask);
		ble_location_pending = false;
		ble_location_initialized = false;
		at_event_send(EVENT_RESTORE_FULL_STACK);
		return;
	}

	LOG_INF("BLE ready (state=%d, link_mask=0x%x, time_sync=%d)",
		loc_ctx->sidewalk_state,
		loc_ctx->link_status.link_status_mask,
		loc_ctx->link_status.time_sync_status);

	/*
	 * L1 (BLE Gateway Location) works differently than L3/L4:
	 * - L3/L4 require device-side scans (WiFi/GNSS) sent to cloud
	 * - L1 is implicit: the BLE gateway's location is used
	 * 
	 * Simply sending any message over BLE will result in the cloud
	 * knowing the device location based on which gateway received it.
	 * 
	 * We'll send a simple "ping" message to trigger this.
	 */
	LOG_INF("Sending BLE location ping message...");

	/* Send a simple message over BLE - the gateway location will be recorded */
	static uint8_t ping_payload[] = { 0x4C, 0x31 };  /* "L1" marker */
	
	struct sid_msg msg = {
		.data = ping_payload,
		.size = sizeof(ping_payload),
	};
	
	struct sid_msg_desc msg_desc = {
		.link_type = SID_LINK_TYPE_1,
		.type = SID_MSG_TYPE_NOTIFY,
		.link_mode = SID_LINK_MODE_CLOUD,
	};

	sid_error_t err = sid_put_msg(loc_ctx->handle, &msg, &msg_desc);
	if (err != SID_ERROR_NONE) {
		LOG_ERR("sid_put_msg (BLE ping) failed: %d", err);
		ble_location_pending = false;
		ble_location_initialized = false;
		at_event_send(EVENT_RESTORE_FULL_STACK);
		return;
	}

	LOG_INF("BLE location ping sent (msg_id=%u)", msg_desc.id);
	LOG_INF("Gateway location will be recorded by cloud.");
	
	/* Mark as complete and restore full stack */
	ble_location_pending = false;
	ble_location_initialized = false;
	
	/* Small delay to let message go out, then restore */
	k_msleep(500);
	at_event_send(EVENT_RESTORE_FULL_STACK);
}
