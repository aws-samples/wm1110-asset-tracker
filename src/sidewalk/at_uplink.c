// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <sid_api.h>
#include <sid_error.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_uplink, CONFIG_TRACKER_LOG_LEVEL);

#include <asset_tracker.h>
#include <sidewalk/at_uplink.h>


void at_send_uplink(at_ctx_t *context) 
{

	at_ctx_t *at_ctx = (at_ctx_t *)context;

	static struct sid_msg msg;
	uint8_t uptype = (uint8_t) at_ctx->uplink_type;
	sid_error_t sid_ret = SID_ERROR_NONE;
	uint8_t payload[MAX_PAYLOAD_SIZE];
	uint8_t payload_len = 0;
	struct sid_msg_desc desc = {
		.type = SID_MSG_TYPE_NOTIFY,
		.link_type = at_ctx->at_conf.sid_link_type,
		.link_mode = SID_LINK_MODE_CLOUD,
	};
	
	//Decode the payloads according to PAYLOADS.md in https://github.com/aws-samples/wm1110-asset-tracker/
	switch (uptype) {
		case CONFIG_T:
			//TODO
			break;

		case NOLOC_T:
			at_ctx->total_msg = 1;
			at_ctx->cur_msg++;
			payload[0] = (uptype<<6);  //Upper two bits is message type.
			payload[1] = at_ctx->sensors.batt;
			payload[2] = (int8_t) at_ctx->sensors.temp;
			payload[3] = (uint8_t) at_ctx->sensors.hum;
			payload[4] = (uint8_t) at_ctx->motion<<7;
			payload[4] |= (uint8_t) at_ctx->sensors.peak_accel;
			payload_len = UPLINK_MSG_HDR_SIZE;

			LOG_HEXDUMP_DBG(payload, payload_len, "noloc_uplink_buffer");

			msg = (struct sid_msg){ .data = payload, .size = payload_len};
			sid_ret = sid_put_msg(at_ctx->handle, &msg, &desc);

			if (SID_ERROR_NONE != sid_ret) {
				LOG_ERR("failed sending message err:%d", (int)sid_ret);
				return;
			}
			LOG_INF("queued NOLOC uplink message id:%u", desc.id);
			break;

		case WIFI_T:
			uint8_t num_aps = at_ctx->wifi_scan_results.nb_results;
			LOG_DBG("number of wifi results available: %d", num_aps);

			if ( at_ctx->total_msg == 0){
				//calculate how many messages are needed based on results available. 2 results per message.
				at_ctx->total_msg = (num_aps + 1) / 2;
				LOG_DBG("total messages to send = %d", at_ctx->total_msg );
				if(at_ctx->total_msg > 0b111) {
					LOG_ERR("failed sending uplink. message count exceeds 3 bits!");
					break;
				}
				
				at_ctx->cur_msg++;
				// Send the first message with the sensor data
				payload[0] = (uint8_t) (uptype<<6);  //Upper two bits is message type.
				payload[0] |= (uint8_t) (at_ctx->total_msg<<3);  //bits 5-3 are total fragments
				payload[1] = at_ctx->sensors.batt;
				payload[2] = (int8_t) at_ctx->sensors.temp;
				payload[3] = (uint8_t) at_ctx->sensors.hum;
				payload[4] = (uint8_t) at_ctx->motion<<7;
				payload[4] |= (uint8_t) at_ctx->sensors.peak_accel;

				if(num_aps == 1) {
					payload_len = UPLINK_MSG_HDR_SIZE + WIFI_AP_ADDRESS_SIZE + WIFI_AP_RSSI_SIZE;
					memcpy(payload + UPLINK_MSG_HDR_SIZE, &at_ctx->wifi_scan_results.rssi[0], WIFI_AP_RSSI_SIZE);
					memcpy(payload + UPLINK_MSG_HDR_SIZE + WIFI_AP_RSSI_SIZE, &at_ctx->wifi_scan_results.mac[0], WIFI_AP_ADDRESS_SIZE);
				} else {
					payload_len = UPLINK_MSG_HDR_SIZE + (2 * (WIFI_AP_ADDRESS_SIZE + WIFI_AP_RSSI_SIZE));
					memcpy(payload + UPLINK_MSG_HDR_SIZE, &at_ctx->wifi_scan_results.rssi[0], WIFI_AP_RSSI_SIZE);
					memcpy(payload + (UPLINK_MSG_HDR_SIZE + WIFI_AP_RSSI_SIZE), &at_ctx->wifi_scan_results.mac[0], WIFI_AP_ADDRESS_SIZE);
					memcpy(payload + (UPLINK_MSG_HDR_SIZE + WIFI_AP_RSSI_SIZE + WIFI_AP_ADDRESS_SIZE), &at_ctx->wifi_scan_results.rssi[1], WIFI_AP_RSSI_SIZE);
					memcpy(payload + (UPLINK_MSG_HDR_SIZE + (2 * WIFI_AP_RSSI_SIZE) + WIFI_AP_ADDRESS_SIZE), &at_ctx->wifi_scan_results.mac[1], WIFI_AP_ADDRESS_SIZE);
				}
				
				LOG_HEXDUMP_DBG(payload, payload_len, "wifi_uplink_buffer");
				
				msg = (struct sid_msg){ .data = payload, .size = payload_len};
				sid_ret = sid_put_msg(at_ctx->handle, &msg, &desc);
				if (SID_ERROR_NONE != sid_ret) {
					LOG_ERR("failed sending WIFI message err:%d", (int)sid_ret);
					return;
				}
				LOG_INF("queued WIFI message %d of %d - id:%u", at_ctx->cur_msg, at_ctx->total_msg, desc.id);
				
			} else {
				//second message if APs>2 - max APs to send = 4
				at_ctx->cur_msg++;
				payload[0] = (uint8_t) (uptype<<6);  //Upper two bits is message type.
				payload[0] |= (uint8_t) (at_ctx->total_msg<<3);  //bits 5-3 are total fragments
				payload[0] |= (uint8_t) (0b111);  //bits 2-0 is frag num (last=0b111) 

				if(num_aps == 3) {
					payload_len = 1 + WIFI_AP_ADDRESS_SIZE + WIFI_AP_RSSI_SIZE;
					memcpy(payload + 1, &at_ctx->wifi_scan_results.rssi[2], WIFI_AP_RSSI_SIZE);
					memcpy(payload + 1 + WIFI_AP_RSSI_SIZE, &at_ctx->wifi_scan_results.mac[2], WIFI_AP_ADDRESS_SIZE);
				} else {
					payload_len = 1 + (2 * (WIFI_AP_ADDRESS_SIZE + WIFI_AP_RSSI_SIZE));
					memcpy(payload + 1, &at_ctx->wifi_scan_results.rssi[2], WIFI_AP_RSSI_SIZE);
					memcpy(payload + (1 + WIFI_AP_RSSI_SIZE), &at_ctx->wifi_scan_results.mac[2], WIFI_AP_ADDRESS_SIZE);
					memcpy(payload + (1 + WIFI_AP_RSSI_SIZE + WIFI_AP_ADDRESS_SIZE), &at_ctx->wifi_scan_results.rssi[3], WIFI_AP_RSSI_SIZE);
					memcpy(payload + (1 + (2 * WIFI_AP_RSSI_SIZE) + WIFI_AP_ADDRESS_SIZE), &at_ctx->wifi_scan_results.mac[3], WIFI_AP_ADDRESS_SIZE);
				}
				
				LOG_HEXDUMP_DBG(payload, payload_len, "wifi_uplink_buffer");
				
				msg = (struct sid_msg){ .data = payload, .size = payload_len};
				sid_ret = sid_put_msg(at_ctx->handle, &msg, &desc);
				if (SID_ERROR_NONE != sid_ret) {
					LOG_ERR("failed sending WIFI message err:%d", (int)sid_ret);
					return;
				}
				LOG_INF("queued WIFI message %d of %d - id:%u", at_ctx->cur_msg, at_ctx->total_msg, desc.id);

			}
			break;

		case GNSS_T:
			if ( at_ctx->total_msg == 0 ){
				//calculate how many messages are needed based on NAV message size (header message + x GNSS data messages)
				LOG_DBG("nav size: %d", at_ctx->gnss_scan_results.nav_msg_size);

				at_ctx->total_msg = 1 + (at_ctx->gnss_scan_results.nav_msg_size  / (MAX_PAYLOAD_SIZE - 1)) + 1;  
				LOG_DBG("Total messages to send: %d", at_ctx->total_msg);
				if(at_ctx->total_msg > 0b111) {
					LOG_ERR("failed sending uplink. message count exceeds 3 bits!");
					break;
				}
				at_ctx->cur_msg++;
				//first message includes header
				payload[0] = (uint8_t) (uptype<<6);  //Upper two bits is message type.
				payload[0] |= (uint8_t) (at_ctx->total_msg<<3);  //bits 5-3 are total fragments
				payload[1] = at_ctx->sensors.batt;
				payload[2] = (int8_t) at_ctx->sensors.temp;
				payload[3] = (uint8_t) at_ctx->sensors.hum;
				payload[4] = (uint8_t) at_ctx->motion<<7;
				payload[4] |= (uint8_t) at_ctx->sensors.peak_accel;
				payload[5] = at_ctx->gnss_scan_results.nav_msg_size;
				payload[6] = (uint8_t) (at_ctx->gnss_scan_results.capture_time >> 24);
				payload[7] = (uint8_t) (at_ctx->gnss_scan_results.capture_time >> 16);
				payload[8] = (uint8_t) (at_ctx->gnss_scan_results.capture_time >> 8);
				payload[9] = (uint8_t) (at_ctx->gnss_scan_results.capture_time & 0xFF);

				payload_len = UPLINK_MSG_HDR_SIZE + 5;
				
				LOG_HEXDUMP_DBG(payload, payload_len, "gnss_uplink_buffer");
				
				msg = (struct sid_msg){ .data = payload, .size = payload_len};
				sid_ret = sid_put_msg(at_ctx->handle, &msg, &desc);
				if (SID_ERROR_NONE != sid_ret) {
					LOG_ERR("failed sending gnss message err:%d", (int)sid_ret);
					return;
				}
				LOG_INF("queued GNSS message %d of %d - id:%u", at_ctx->cur_msg, at_ctx->total_msg, desc.id);
				at_ctx->gnss_scan_results.gnss_index = 1;  //drop the first byte (frame count?)
			} else {
				// GNSS nav frags
				at_ctx->cur_msg++;
				payload[0] = (uint8_t) (uptype<<6);  //Upper two bits is message type.
				payload[0] |= (uint8_t) (at_ctx->total_msg<<3);  //bits 5-3 are total fragments
				payload[0] |= (uint8_t) ((at_ctx->cur_msg == at_ctx->total_msg) ? (7) : at_ctx->cur_msg);  //bits 2-0 is frag num (last=0b111(7)) 

				payload_len = ((at_ctx->gnss_scan_results.nav_msg_size - at_ctx->gnss_scan_results.gnss_index) > (MAX_PAYLOAD_SIZE - 1)) ? MAX_PAYLOAD_SIZE : (at_ctx->gnss_scan_results.nav_msg_size - at_ctx->gnss_scan_results.gnss_index + 1 );
				memcpy(payload + 1, (at_ctx->gnss_scan_results.nav_msg + at_ctx->gnss_scan_results.gnss_index), (payload_len - 1));
				
				at_ctx->gnss_scan_results.gnss_index += MAX_PAYLOAD_SIZE - 1;
				
				LOG_HEXDUMP_DBG(payload, payload_len, "gnss_uplink_buffer");
				
				msg = (struct sid_msg){ .data = payload, .size = payload_len};
				sid_ret = sid_put_msg(at_ctx->handle, &msg, &desc);
				if (SID_ERROR_NONE != sid_ret) {
					LOG_ERR("failed sending gnss message err:%d", (int)sid_ret);
					return;
				}
				LOG_INF("queued GNSS message %d of %d - id:%u", at_ctx->cur_msg, at_ctx->total_msg, desc.id);
		
			}
			break;

		default:
			LOG_ERR("Invalid uplink type!");
	}
}

void at_msg_sent(at_ctx_t *context) {
	
	at_ctx_t *at_ctx = (at_ctx_t *)context;

	if (at_ctx->total_msg > 0) {
		// done sending message
		if (at_ctx->cur_msg < at_ctx->total_msg) {
			// send next message
			at_event_send(EVENT_SEND_UPLINK);
		} else {
			// uplink complete
			at_event_send(EVENT_UPLINK_COMPLETE);
			at_ctx->total_msg = 0;	
			at_ctx->cur_msg = 0;
		}
	}
}

void at_send_error(at_ctx_t *context) {
	at_ctx_t *at_ctx = (at_ctx_t *)context;

	if (at_ctx->total_msg > 0) {
		// TODO: retries
		LOG_ERR("Error sending messages... aborting!");
		at_event_send(EVENT_UPLINK_COMPLETE);
		at_ctx->total_msg = 0;	
		at_ctx->cur_msg = 0;
	}

}

