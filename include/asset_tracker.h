// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef ASSET_TRACKER_H
#define ASSET_TRACKER_H

#include <sid_api.h>

#define BLE_LM (uint32_t)(SID_LINK_TYPE_1)
#define FSK_LM (uint32_t)(SID_LINK_TYPE_2)
#define LORA_LM (uint32_t)(SID_LINK_TYPE_3)

#define DEFAULT_AUTO_SCAN_INTERVAL		30		/* seconds */

#define MAX_PAYLOAD_SIZE 19 			// Max payload size limited to Sidewalk CSS limit

#define RECEIVE_TASK_STACK_SIZE (4096)
#define RECEIVE_TASK_PRIORITY (CONFIG_SIDEWALK_THREAD_PRIORITY + 1)

#define LINK_DOWN 0
#define LINK_UP 1

/**
 * Sensor telemetry data structure
 */
struct at_sensors {
	uint8_t batt;
	double temp;
	double hum;
	double max_accel_x;
	double max_accel_y;
	double max_accel_z;
	double peak_accel;
};

/**
 * Sidewalk connection states
 */
enum at_sidewalk_state {
	STATE_SIDEWALK_INIT,
	STATE_SIDEWALK_READY,
	STATE_SIDEWALK_NOT_READY,
	STATE_SIDEWALK_SECURE_CONNECTION,
};

/**
 * Application states
 */
enum at_state {
	AT_STATE_INIT,
	AT_STATE_RUN
};

/**
 * Link status tracking
 */
struct link_status {
	enum sid_time_sync_status time_sync_status;
	uint32_t link_status_mask;
	uint32_t supported_link_mode[SID_LINK_TYPE_MAX_IDX];
};

/**
 * Application configuration
 */
struct at_config {
	uint16_t max_rec;
	uint32_t sid_link_type;
	uint8_t motion_period;
	uint8_t scan_freq_motion;
	uint8_t motion_thres;
	uint8_t scan_freq_static;
};

/**
 * Main application context
 */
typedef struct at_context {
	struct sid_event_callbacks event_callbacks;
	struct sid_config sidewalk_config;
	struct sid_handle *handle;
	enum at_sidewalk_state sidewalk_state;
	struct link_status link_status;
	bool sidewalk_registered;
	bool stack_started;
	bool ble_location_pending;  // Waiting for BLE ready to trigger L1 location
	enum at_state state;
	bool connection_request;
	bool motion;
	uint8_t total_msg;
	uint8_t cur_msg;
	struct at_sensors sensors;
	struct at_config at_conf;
} at_ctx_t;

/**
 * Application events - simplified for SDK-based location
 */
typedef enum at_events {
	SIDEWALK_EVENT,
	BUTTON_EVENT_SHORT,
	BUTTON_EVENT_LONG,
	MOTION_EVENT,
	EVENT_RADIO_SWITCH,
	EVENT_BLE_CONNECTION_REQUEST,
	EVENT_BLE_CONNECTION_WAIT,
	EVENT_SCAN_LOC,           // Triggers SDK location scan
	EVENT_SEND_UPLINK,        // Send sensor telemetry
	EVENT_SCAN_SENSORS,
	EVENT_CONFIG_UPDATE,
	EVENT_SID_START,
	EVENT_SID_STOP,
	EVENT_UPLINK_COMPLETE,
	EVENT_BLE_LOCATION_START,   // Switch to BLE-only mode for L1 location
	EVENT_BLE_LOCATION_READY,   // BLE stack ready, trigger L1 location
	EVENT_RESTORE_FULL_STACK,   // Restore full stack after BLE location
} at_event_t;

/**
 * Received message structure
 */
struct at_rx_msg {
	uint16_t msg_id;
	size_t pld_size;
	uint8_t rx_payload[MAX_PAYLOAD_SIZE];
};

/* Public API */
void at_event_send(at_event_t event);
void at_rx_task_msg_q_write(struct at_rx_msg *rx_msg);
sid_error_t at_thread_init(void);

#endif /* ASSET_TRACKER_H */
