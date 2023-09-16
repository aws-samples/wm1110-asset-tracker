// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <math.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <halo_lr11xx_radio.h>
#include <asset_tracker.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(lr1110_wifi, CONFIG_TRACKER_LOG_LEVEL);

static at_ctx_t *atcontext;

/* often one of the access points received is a mobile-AP, so get the max +1 */
#define WIFI_MAX_RESULTS ( 10 )

/**
 * @brief Minimal number of detected access point in a scan result to consider the scan valid
 */
#define WIFI_SCAN_NB_AP_MIN ( 2 )

/*!
 * @brief Structure representing a single scan result
 */
typedef struct
{
	lr11xx_wifi_mac_address_t mac_address;	//!< MAC address of the Wi-Fi access point which has been detected
	lr11xx_wifi_channel_t channel;		//!< Channel on which the access point has been detected
	lr11xx_wifi_signal_type_result_t type;			//!< Type of Wi-Fi which has been detected
	int8_t rssi;			//!< Strength of the detected signal
} wifi_scan_single_result_t;

/*!
 * @brief Structure representing a collection of scan results
 */
typedef struct
{
	uint8_t nbr_results;					//!< Number of results
	uint32_t power_consumption_uah;		//!< Power consumption to acquire this set of results
	uint32_t timestamp;					//!< Timestamp at which the data set has been completed
	wifi_scan_single_result_t results[WIFI_MAX_RESULTS];	//!< Buffer containing the results
} wifi_scan_all_result_t;

wifi_configuration_scan_t wifi_configuration = {
	.signal_type			= LR11XX_WIFI_TYPE_SCAN_B,
	.base.channel_mask		= 0x0421,
	.scan_mode				= LR11XX_WIFI_SCAN_MODE_BEACON,
	.base.max_result		= WIFI_MAX_RESULTS,
	.nb_scan_per_channel	= 10,
	.timeout_per_scan		= 90,
	.abort_on_timeout		= true,
};

static lr11xx_wifi_basic_complete_result_t wifi_results_mac_addr[WIFI_MAX_RESULTS];
static wifi_scan_all_result_t wifi_results;

const char* lr11xx_wifi_signal_type_result_to_str(lr11xx_wifi_signal_type_result_t val)
{
	switch (val) {
		case LR11XX_WIFI_TYPE_RESULT_B: return "LR11XX_WIFI_TYPE_RESULT_B";
		case LR11XX_WIFI_TYPE_RESULT_G: return "LR11XX_WIFI_TYPE_RESULT_G";
		case LR11XX_WIFI_TYPE_RESULT_N: return "LR11XX_WIFI_TYPE_RESULT_N";
		default: return "unknown";
	}
}

const char* lr11xx_wifi_frame_type_to_str( const lr11xx_wifi_frame_type_t value )
{
	switch (value) {
		case LR11XX_WIFI_FRAME_TYPE_MANAGEMENT: return "management";
		case LR11XX_WIFI_FRAME_TYPE_CONTROL: return "control";
		case LR11XX_WIFI_FRAME_TYPE_DATA: return "data";
		default: return "unknown";
	}
}

int smtc_wifi_get_results(void *drv_ctx, wifi_scan_all_result_t* wifi_results)
{
	lr11xx_status_t status;
	uint8_t nb_results;
	uint8_t max_nb_results;
	uint8_t result_index = 0;

	status = lr11xx_wifi_get_nb_results(drv_ctx, &nb_results);
	if (status != LR11XX_STATUS_OK) {
		LOG_ERR("lr11xx_wifi_get_nb_results() fail");
		return -1;
	}
	LOG_INF("scanned nb_results %u", nb_results);

	/* check if the array is big enough to hold all results */
	max_nb_results = sizeof( wifi_results_mac_addr ) / sizeof( wifi_results_mac_addr[0] );
	if (nb_results > max_nb_results)
	{   
		LOG_ERR("Wi-Fi scan result size exceeds %u (%u)", max_nb_results, nb_results);
		return -1;
	}

	memset( wifi_results_mac_addr, 0, sizeof wifi_results_mac_addr );
	status = lr11xx_wifi_read_basic_complete_results(drv_ctx, 0, nb_results, wifi_results_mac_addr);
	if (status != LR11XX_STATUS_OK) {
		LOG_ERR("lr11xx_wifi_read_basic_complete_results() fail");
		return -1;
	}

	/* add scan to results */
	for( uint8_t index = 0; index < nb_results; index++ )
	{ 
		const lr11xx_wifi_basic_complete_result_t* local_basic_result = &wifi_results_mac_addr[index];
		lr11xx_wifi_channel_t		channel;
		bool						rssi_validity;
		lr11xx_wifi_mac_origin_t	mac_origin_estimation;

		lr11xx_wifi_parse_channel_info( local_basic_result->channel_info_byte, &channel, &rssi_validity,
							&mac_origin_estimation );

		if( mac_origin_estimation != LR11XX_WIFI_ORIGIN_BEACON_MOBILE_AP )
		{
			wifi_results->results[result_index].channel = channel;

			wifi_results->results[result_index].type =
				lr11xx_wifi_extract_signal_type_from_data_rate_info( local_basic_result->data_rate_info_byte );

			memcpy( wifi_results->results[result_index].mac_address, local_basic_result->mac_address,
					LR11XX_WIFI_MAC_ADDRESS_LENGTH );

			wifi_results->results[result_index].rssi = local_basic_result->rssi;
			{
				const uint8_t *mac = local_basic_result->mac_address;
				LOG_INF("%u) ch%u %02x:%02x:%02x:%02x:%02x:%02x %ddBm", index, channel, mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], local_basic_result->rssi);
			}
			wifi_results->nbr_results++;
			result_index++;
		} else{
			LOG_DBG("%u) mobile-AP", index);
			//do not add mobile-aps to the results
		}

	}

	return 0;
}

static int wifi_mw_send_results(at_ctx_t *app_ctx)
{
	LOG_INF("valid nb_results %u", wifi_results.nbr_results);
	/* Check if there are results to be sent */
	if (wifi_results.nbr_results < WIFI_SCAN_NB_AP_MIN) {
		LOG_INF("only %u results. Sending NOLOC message instead.", wifi_results.nbr_results);
		atcontext->uplink_type = NOLOC_T;
		at_event_send(EVENT_SEND_UPLINK);
		return 0;
	}

	atcontext->wifi_scan_results.nb_results = wifi_results.nbr_results;
	LOG_INF("at nb_results %u", atcontext->wifi_scan_results.nb_results );

	// add scan results
	for( uint8_t i = 0; i < wifi_results.nbr_results; i++ )
	{
		atcontext->wifi_scan_results.rssi[i] = wifi_results.results[i].rssi;
		memcpy( &atcontext->wifi_scan_results.mac[i], &wifi_results.results[i].mac_address, WIFI_AP_ADDRESS_SIZE );
		LOG_DBG("%u) %02x:%02x:%02x:%02x:%02x:%02x", i, atcontext->wifi_scan_results.mac[i][0], atcontext->wifi_scan_results.mac[i][1], atcontext->wifi_scan_results.mac[i][2], atcontext->wifi_scan_results.mac[i][3], atcontext->wifi_scan_results.mac[i][4], atcontext->wifi_scan_results.mac[i][5]);
	}

	atcontext->uplink_type = WIFI_T;
	at_event_send(EVENT_SEND_UPLINK);

	return 0;
}

void on_wifi_scan_done(void *arg)
{
	void *drv_ctx = lr11xx_get_drv_ctx();
	memset( &wifi_results, 0, sizeof wifi_results );
	int ret = smtc_wifi_get_results(drv_ctx, &wifi_results);
	if (ret < 0) {
		LOG_ERR("smtc_wifi_get_results() fail");
		return;
	}

	wifi_mw_send_results(arg);
}

int scan_wifi(at_ctx_t *app_ctx)
{
	atcontext = app_ctx;

	LOG_INF("Scanning for WiFi access points...");
	if(atcontext->at_conf.workshop_mode==true) {
		//if in workshop mode, just call scan done and populate the buffer with the pre seeded MACs
		uint8_t random = (uint8_t) (0xFF & k_uptime_get_32());
		int8_t rssi_jit[2];
	
		//jitter the workshop rssi results a bit
		rssi_jit[0] = atcontext->at_conf.workshop_wifi_rssi[0] + (random>>4); 
		rssi_jit[1] = atcontext->at_conf.workshop_wifi_rssi[1] - (random>>4);

		//MAC 1
		atcontext->wifi_scan_results.rssi[0] = rssi_jit[0];
		memcpy( &atcontext->wifi_scan_results.mac[0], &atcontext->at_conf.workshop_wifi_macs[0], WIFI_AP_ADDRESS_SIZE );
		//MAC 2
		atcontext->wifi_scan_results.rssi[1] = rssi_jit[1];
		memcpy( &atcontext->wifi_scan_results.mac[1], &atcontext->at_conf.workshop_wifi_macs[1], WIFI_AP_ADDRESS_SIZE );

		atcontext->wifi_scan_results.nb_results = 2;
	
		LOG_INF("Static workshop MACs found:");
		LOG_INF("1) ch6 %02x %02x %02x %02x %02x %02x %ddBm", \
						atcontext->at_conf.workshop_wifi_macs[0][0], \
						atcontext->at_conf.workshop_wifi_macs[0][1], \
						atcontext->at_conf.workshop_wifi_macs[0][2], \
						atcontext->at_conf.workshop_wifi_macs[0][3], \
						atcontext->at_conf.workshop_wifi_macs[0][4], \
						atcontext->at_conf.workshop_wifi_macs[0][5], \
						rssi_jit[0]);
		LOG_INF("2) ch11 %02x %02x %02x %02x %02x %02x %ddBm", \
						atcontext->at_conf.workshop_wifi_macs[1][0], \
						atcontext->at_conf.workshop_wifi_macs[1][1], \
						atcontext->at_conf.workshop_wifi_macs[1][2], \
						atcontext->at_conf.workshop_wifi_macs[1][3], \
						atcontext->at_conf.workshop_wifi_macs[1][4], \
						atcontext->at_conf.workshop_wifi_macs[1][5], \
						rssi_jit[1]);

		atcontext->uplink_type = WIFI_T;
		at_event_send(EVENT_SEND_UPLINK);
		return 0;
	} else {
		//if not in workshop mode, perform a real scan

		void *drv_ctx = lr11xx_get_drv_ctx();
		if (lr11xx_system_wakeup(drv_ctx) != LR11XX_STATUS_OK) {
			LOG_ERR("scan_wifi wake-up fail");
			return -1;
		}

	#if defined(CONFIG_LR11XX_CLI)
		lr11xx_status_t status = lr11xx_wifi_scan(drv_ctx,
			wifi_configuration.signal_type,
			wifi_configuration.base.channel_mask,
			wifi_configuration.scan_mode,
			wifi_configuration.base.max_result,
			wifi_configuration.nb_scan_per_channel,
			wifi_configuration.timeout_per_scan,
			wifi_configuration.abort_on_timeout
		);
	#else
		lr11xx_status_t status = lr11xx_wifi_scan(drv_ctx,
			LR11XX_WIFI_TYPE_SCAN_B, /* const lr11xx_wifi_signal_type_scan_t signal_type */
			0x0421, /* const lr11xx_wifi_channel_mask_t channels */
			LR11XX_WIFI_SCAN_MODE_BEACON, /* const lr11xx_wifi_mode_t scan_mode */
			4, /* const uint8_t max_results */
			10, /* const uint8_t nb_scan_per_channel */
			90, /* const uint16_t timeout_in_ms */
			true /* const bool abort_on_timeout */
		);
	#endif
		if (status != LR11XX_STATUS_OK) {
			LOG_ERR("lr11xx_wifi_scan fail");
			return -1;
		} else
			return 0;
	}
}