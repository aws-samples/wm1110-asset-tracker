// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef ASSET_TRACKER_H
#define ASSET_TRACKER_H

#include <sid_api.h>

#define BLE_LM (uint32_t)(SID_LINK_TYPE_1)
#define FSK_LM (uint32_t)(SID_LINK_TYPE_2)
#define LORA_LM (uint32_t)(SID_LINK_TYPE_3)

#define NB_MAX_SV		8	/* GNSS */
#define DEFAULT_AUTO_SCAN_INTERVAL		30		/* seconds */

#define FRAGMENT_TYPE_GNSS         0	// to cloud port 198
#define FRAGMENT_TYPE_WIFI         1	// to cloud port 197
#define FRAGMENT_TYPE_MODEM_MSG    2	// to cloud port 199
#define FRAGMENT_TYPE_GNSS_NG      3	// to cloud port 192

#define MAX_PAYLOAD_SIZE 19 			//max payload size limited to Sidewalk CSS limit

#define RECEIVE_TASK_STACK_SIZE (4096)
#define RECEIVE_TASK_PRIORITY (CONFIG_SIDEWALK_THREAD_PRIORITY + 1)

#define STATIC_WS_MAC1 {0x04, 0x18, 0xD6, 0x36, 0xF1, 0xEC}
#define STATIC_WS_RSSI1 -55
#define STATIC_WS_MAC2 {0x04, 0x18, 0xD6, 0x8C, 0xCD, 0x6E}
#define STATIC_WS_RSSI2 -66

#define MAX_WIFI_NB ( 8 )    //max number of wifi results we will send
#define MAX_GNSS_SIZE (300)  //max size of GNSS nav buffer
#define GNSS_DEBUG

#define WIFI_AP_ADDRESS_SIZE ( 6 ) //Size in bytes of a WiFi Access-Point address
#define WIFI_AP_RSSI_SIZE ( 1 )	//Size in bytes to store the RSSI of a detected WiFi Access-Point

#define UPLINK_MSG_HDR_SIZE ( 5 ) //Size in bytes of uplink message header

#define LINK_DOWN 0
#define LINK_UP 1

typedef enum uplink_msg_types {
	CONFIG_T,
	NOLOC_T,
	WIFI_T,
	GNSS_T,
	UNSET_T,
} uplink_msg_t;

typedef enum loc_scan_modes {
	WIFI,
	GNSS,
	GNSS_WIFI,
} loc_scan_t;

typedef enum gnss_scan_mode {
	GNSS_ASSISTED,
	GNSS_AUTONOMOUS,
} gnss_scan_mode_t;

struct at_sensors {
	uint8_t batt;
	double temp;
	double hum;
	double max_accel_x;
	double max_accel_y;
	double max_accel_z;
	double peak_accel;
};

enum at_sidewalk_state {
	STATE_SIDEWALK_INIT,
	STATE_SIDEWALK_READY,
	STATE_SIDEWALK_NOT_READY,
	STATE_SIDEWALK_SECURE_CONNECTION,
};

enum at_state {
	AT_STATE_INIT,
	AT_STATE_RUN
};

struct link_status {
	enum sid_time_sync_status time_sync_status;
	uint32_t link_status_mask;
	uint32_t supported_link_mode[SID_LINK_TYPE_MAX_IDX];
};

struct wifi_scan_res{
	uint8_t nb_results;
	uint8_t mac[MAX_WIFI_NB][WIFI_AP_ADDRESS_SIZE];
	int8_t rssi[MAX_WIFI_NB];
};

struct gnss_scan_res{
	uint8_t nb_sv;					//number of sats seen
	uint32_t capture_time;
	uint8_t nav_msg_size;
	uint8_t gnss_index;
	const uint8_t *nav_msg; 
};

struct at_config{
    uint16_t max_rec;
    uint32_t sid_link_type;
    loc_scan_t loc_scan_mode;
	gnss_scan_mode_t gnss_scan_mode;
    uint8_t motion_period;
    uint8_t scan_freq_motion;
    uint8_t motion_thres;
    uint8_t scan_freq_static;
	bool workshop_mode;
	uint8_t workshop_wifi_macs [2][6];
	int8_t workshop_wifi_rssi [2];
};

typedef struct at_context {
	struct sid_event_callbacks event_callbacks;
	struct sid_config sidewalk_config;
	struct sid_handle *handle;
	enum at_sidewalk_state sidewalk_state;
	struct link_status link_status;
	bool sidewalk_registered;
	enum at_state state;
	uplink_msg_t uplink_type;
	bool connection_request;
	bool motion;
	uint8_t total_msg;
	uint8_t cur_msg;
	struct at_sensors sensors;
    struct at_config at_conf;
	struct wifi_scan_res wifi_scan_results;
	struct gnss_scan_res gnss_scan_results;
} at_ctx_t;

typedef enum at_events {
    SIDEWALK_EVENT,
	BUTTON_EVENT_SHORT,
	BUTTON_EVENT_LONG,
    MOTION_EVENT,
	EVENT_SWITCH_WS_MODE,
	EVENT_RADIO_SWITCH,
	EVENT_BLE_CONNECTION_REQUEST,
	EVENT_BLE_CONNECTION_WAIT,
	EVENT_SCAN_LOC,
	EVENT_SEND_UPLINK,
    EVENT_SCAN_SENSORS,
	EVENT_SCAN_WIFI,
    EVENT_SCAN_GNSS,
    EVENT_SCAN_RESULT_STORE,
	EVENT_SCAN_RESULT_SEND,
	EVENT_CONFIG_UPDATE,
    EVENT_SID_START,
    EVENT_SID_STOP,
	EVENT_UPLINK_COMPLETE,
} at_event_t;

struct at_rx_msg {
	uint16_t msg_id;
	size_t pld_size;
	uint8_t rx_payload[MAX_PAYLOAD_SIZE];
};

void at_event_send(at_event_t event);
void at_rx_task_msg_q_write(struct at_rx_msg *rx_msg);

sid_error_t at_thread_init(void);

int scan_wifi(at_ctx_t *app_ctx);

int gnss_scan_timer_set(unsigned sec);

void start_gnss_scan(at_ctx_t *app_ctx);

#endif /* ASSET_TRACKER_H */