// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <sid_api.h>
#include <sid_error.h>
#include <sid_hal_reset_ifc.h>

#if defined(CONFIG_SIDEWALK_CLI)
#include <sid_shell.h>
#endif

#include <asset_tracker.h>
#include "peripherals/at_timers.h"
#include <sidewalk/at_uplink.h>

#include <zephyr/logging/log.h>

LOG_MODULE_REGISTER(callbacks, CONFIG_TRACKER_LOG_LEVEL);

static const uint8_t *status_name[] = { "ready", "not ready", "Error", "secure channel ready" };

static const uint8_t *link_mode_idx_name[] = { "ble", "fsk", "lora" };

static void on_sidewalk_event(bool in_isr, void *context)
{
	LOG_DBG("on event, from %s, context %p", in_isr ? "ISR" : "App", context);
	at_event_send(SIDEWALK_EVENT);
}

static void on_sidewalk_msg_received(const struct sid_msg_desc *msg_desc, const struct sid_msg *msg,
				     void *context)
{
#ifdef CONFIG_SIDEWALK_CLI
	CLI_register_message_received(msg_desc->id);
#endif

	LOG_INF("received message(type: %d, link_mode: %d, id: %u size %u)", (int)msg_desc->type,
		(int)msg_desc->link_mode, msg_desc->id, msg->size);
	LOG_HEXDUMP_DBG((uint8_t *)msg->data, msg->size, "Message data: ");
	
	// struct at_rx_msg rx_msg = {
	// 	.msg_id = msg_desc->id,
	// 	.pld_size = msg->size,
	// };
	// memcpy(rx_msg.rx_payload, msg->data, MIN(msg->size, MAX_PAYLOAD_SIZE));
	// at_rx_task_msg_q_write(&rx_msg);
}

static void on_sidewalk_msg_sent(const struct sid_msg_desc *msg_desc, void *context)
{
#ifdef CONFIG_SIDEWALK_CLI
	CLI_register_message_send();
#endif
	LOG_INF("sent message to Sidewalk(type: %d, id: %u)", (int)msg_desc->type, msg_desc->id);
	at_msg_sent(context);
}

static void on_sidewalk_send_error(sid_error_t error, const struct sid_msg_desc *msg_desc,
				   void *context)
{
#ifdef CONFIG_SIDEWALK_CLI
	CLI_register_message_not_send();
#endif
	LOG_ERR("failed to send message(type: %d, id: %u), err:%d", (int)msg_desc->type,
		msg_desc->id, (int)error);
	at_send_error(context);
}

static void on_sidewalk_status_changed(const struct sid_status *status, void *context)
{
	LOG_DBG("Sidewalk status changed: %s", status_name[status->state]);

	at_ctx_t *at_ctx = (at_ctx_t *)context;

#ifdef CONFIG_SIDEWALK_CLI
	CLI_register_sid_status(status);
#endif
	switch (status->state) {
	case SID_STATE_READY:
		at_ctx->sidewalk_state = STATE_SIDEWALK_READY;
		break;
	case SID_STATE_NOT_READY:
		at_ctx->sidewalk_state = STATE_SIDEWALK_NOT_READY;
		break;
	case SID_STATE_ERROR:
		LOG_ERR("Sidewalk error: %d", (int)sid_get_error(at_ctx->handle));
		break;
	case SID_STATE_SECURE_CHANNEL_READY:
		break;
	}


	if ( at_ctx->sidewalk_registered == false && status->detail.registration_status == SID_STATUS_REGISTERED){
		LOG_INF("Device registered on Sidewalk network. Awaiting time sync...");
		at_ctx->sidewalk_registered = true;
	}

	if ( at_ctx->sidewalk_registered == false ) {
		LOG_INF("Awaiting device registration on the Sidewalk network...");
	}
	
    
	at_ctx->link_status.link_status_mask = status->detail.link_status_mask;
	at_ctx->link_status.time_sync_status = status->detail.time_sync_status;

	if (at_ctx->sidewalk_state == STATE_SIDEWALK_READY) {
		if(at_ctx->state == AT_STATE_INIT) {
			at_ctx->state = AT_STATE_RUN;
			LOG_INF("Device time syncronized to Sidewalk network time.  Asset tracker running...");
			// scan_timer_set_and_run(K_MSEC(at_ctx->at_conf.scan_freq_motion));
			scan_timer_set_and_run(K_MSEC(5000));
			at_event_send(EVENT_SID_STOP);
		}
	}

	LOG_DBG("Sidewalk device %sregistered, Time Sync %s, Link status: {BLE: %s, FSK: %s, LoRa: %s}",
		(SID_STATUS_REGISTERED == status->detail.registration_status) ? "Is " : "Un",
		(SID_STATUS_TIME_SYNCED == status->detail.time_sync_status) ? "Success" : "Fail",
		(status->detail.link_status_mask & SID_LINK_TYPE_1) ? "Up" : "Down",
		(status->detail.link_status_mask & SID_LINK_TYPE_2) ? "Up" : "Down",
		(status->detail.link_status_mask & SID_LINK_TYPE_3) ? "Up" : "Down");

	for (int i = 0; i < SID_LINK_TYPE_MAX_IDX; i++) {
		enum sid_link_mode mode =
			(enum sid_link_mode)status->detail.supported_link_modes[i];

		if (mode) {
			LOG_DBG("Link mode on %s = {Cloud: %s, Mobile: %s}", link_mode_idx_name[i],
				(mode & SID_LINK_MODE_CLOUD) ? "True" : "False",
				(mode & SID_LINK_MODE_MOBILE) ? "True" : "False");
		}
	}
}

static void on_sidewalk_factory_reset(void *context)
{
	ARG_UNUSED(context);

	LOG_INF("factory reset notification received from sid api");
	if (sid_hal_reset(SID_HAL_RESET_NORMAL)) {
		LOG_WRN("Reboot type not supported");
	}
}

sid_error_t sidewalk_callbacks_set(void *context, struct sid_event_callbacks *callbacks)
{
	if (!callbacks) {
		return SID_ERROR_INVALID_ARGS;
	}
	callbacks->context = context;
	callbacks->on_event = on_sidewalk_event;
	callbacks->on_msg_received = on_sidewalk_msg_received; /* Called from sid_process() */
	callbacks->on_msg_sent = on_sidewalk_msg_sent; /* Called from sid_process() */
	callbacks->on_send_error = on_sidewalk_send_error; /* Called from sid_process() */
	callbacks->on_status_changed = on_sidewalk_status_changed; /* Called from sid_process() */
	callbacks->on_factory_reset = on_sidewalk_factory_reset; /* Called from sid_process() */

	return SID_ERROR_NONE;
}
