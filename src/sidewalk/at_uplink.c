// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <sid_api.h>
#include <sid_error.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_uplink, CONFIG_TRACKER_LOG_LEVEL);

#include <asset_tracker.h>
#include <sidewalk/at_uplink.h>

/**
 * Simplified sensor telemetry payload format (5 bytes):
 * Byte 0: Message type (upper 2 bits) | Reserved (lower 6 bits)
 * Byte 1: Battery level (0-100%)
 * Byte 2: Temperature (signed, degrees C)
 * Byte 3: Humidity (0-100%)
 * Byte 4: Motion flag (bit 7) | Peak acceleration (bits 0-6)
 */
#define SENSOR_TELEMETRY_SIZE 5
#define MSG_TYPE_SENSOR_TELEMETRY 0x01

void at_send_uplink(at_ctx_t *context) 
{
	at_ctx_t *at_ctx = (at_ctx_t *)context;

	static struct sid_msg msg;
	sid_error_t sid_ret = SID_ERROR_NONE;
	uint8_t payload[SENSOR_TELEMETRY_SIZE];
	
	struct sid_msg_desc desc = {
		.type = SID_MSG_TYPE_NOTIFY,
		.link_type = at_ctx->at_conf.sid_link_type,
		.link_mode = SID_LINK_MODE_CLOUD,
	};
	
	// Build sensor telemetry payload
	at_ctx->total_msg = 1;
	at_ctx->cur_msg = 1;
	
	payload[0] = (MSG_TYPE_SENSOR_TELEMETRY << 6);  // Message type in upper 2 bits
	payload[1] = at_ctx->sensors.batt;
	payload[2] = (int8_t)at_ctx->sensors.temp;
	payload[3] = (uint8_t)at_ctx->sensors.hum;
	payload[4] = (uint8_t)(at_ctx->motion << 7);
	payload[4] |= (uint8_t)((int)at_ctx->sensors.peak_accel) & 0x7F;

	LOG_HEXDUMP_DBG(payload, SENSOR_TELEMETRY_SIZE, "sensor_telemetry_payload");

	msg = (struct sid_msg){ .data = payload, .size = SENSOR_TELEMETRY_SIZE };
	sid_ret = sid_put_msg(at_ctx->handle, &msg, &desc);

	if (SID_ERROR_NONE != sid_ret) {
		LOG_ERR("Failed sending sensor telemetry, err:%d", (int)sid_ret);
		at_ctx->total_msg = 0;
		at_ctx->cur_msg = 0;
		return;
	}
	
	LOG_INF("Queued sensor telemetry uplink, id:%u (batt=%d%%, temp=%dC, hum=%d%%, motion=%d)", 
		desc.id, 
		at_ctx->sensors.batt,
		(int8_t)at_ctx->sensors.temp,
		(uint8_t)at_ctx->sensors.hum,
		at_ctx->motion);
}

void at_msg_sent(at_ctx_t *context) 
{
	at_ctx_t *at_ctx = (at_ctx_t *)context;

	if (at_ctx->total_msg > 0) {
		if (at_ctx->cur_msg < at_ctx->total_msg) {
			// Send next message (if any)
			at_event_send(EVENT_SEND_UPLINK);
		} else {
			// Uplink complete
			at_event_send(EVENT_UPLINK_COMPLETE);
			at_ctx->total_msg = 0;	
			at_ctx->cur_msg = 0;
		}
	}
}

void at_send_error(at_ctx_t *context) 
{
	at_ctx_t *at_ctx = (at_ctx_t *)context;

	if (at_ctx->total_msg > 0) {
		LOG_ERR("Error sending message, aborting uplink");
		at_event_send(EVENT_UPLINK_COMPLETE);
		at_ctx->total_msg = 0;	
		at_ctx->cur_msg = 0;
	}
}
