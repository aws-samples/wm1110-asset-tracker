// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

/*!
 * @file      gnss.c
 *
 * @brief     GNSS LR11xx application layer, initiate scan & send
 *
 * The Clear BSD License
 * Copyright Semtech Corporation 2022. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted (subject to the limitations in the disclaimer
 * below) provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Semtech corporation nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE GRANTED BY
 * THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SEMTECH CORPORATION BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <math.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <halo_lr11xx_radio.h>
#include <asset_tracker.h>

static at_ctx_t *atcontext;

// #ifdef LR11xx
lr11xx_gnss_result_t gnss_result;
#define ASSIST_LATITUDE		30.39
#define ASSIST_LONGITUDE	-98.06
lr11xx_gnss_solver_assistance_position_t assistance_position = { ASSIST_LATITUDE, ASSIST_LONGITUDE };

// minimum number of sattelites required to consider the nav message accurate enough to send
#define MIN_NB_SV 5
// #endif /* LR11xx */

extern lr11xx_gnss_result_t gnss_result;

LOG_MODULE_REGISTER(lr1110_gnss, CONFIG_TRACKER_LOG_LEVEL);

void on_gnss_scan_done(void *arg)
{
	// at_ctx_t *app_ctx = arg;
	uint8_t n_sv_detected = 0;
	void *drv_ctx = lr11xx_get_drv_ctx();

	if (atcontext == NULL) {
		LOG_ERR("on_gnss_scan_done no context");
		return;
	}

	lr11xx_status_t status = lr11xx_gnss_get_result_size(drv_ctx, &gnss_result.length);
	if (status != LR11XX_STATUS_OK) {
		LOG_ERR("gnss_get_result_size fail");
		return;
	}
#ifdef GNSS_DEBUG
	LOG_INF("result size %d", gnss_result.length);
#endif /* GNSS_DEBUG */
	if (gnss_result.length >= GNSS_RESULT_SIZE) {
		LOG_ERR("result too big %d > %d", gnss_result.length, GNSS_RESULT_SIZE);
		return;
	}

	status = lr11xx_gnss_read_results(drv_ctx, gnss_result.buffer, gnss_result.length);
	if (status != LR11XX_STATUS_OK) {
		LOG_ERR("gnss_read_results fail");
		return;
	}

	lr11xx_gnss_get_nb_detected_satellites(drv_ctx, &n_sv_detected);
#ifdef GNSS_DEBUG
	lr11xx_gnss_detected_satellite_t sv_detected[NB_MAX_SV] = { 0 };
	LOG_INF("on_gnss_scan_done %d SV", n_sv_detected);
	lr11xx_gnss_get_detected_satellites( drv_ctx, n_sv_detected, sv_detected );
	for( uint8_t index_sv = 0; index_sv < n_sv_detected; index_sv++ )
	{
		const lr11xx_gnss_detected_satellite_t* local_sv = &sv_detected[index_sv];
		LOG_INF( "  - SV %u: CNR: %i, doppler: %i", local_sv->satellite_id, local_sv->cnr,
			local_sv->doppler );
	}
	LOG_HEXDUMP_INF(gnss_result.buffer, gnss_result.length, "nav");
#endif /* GNSS_DEBUG */

	if (n_sv_detected < MIN_NB_SV) {
		if(atcontext->at_conf.loc_scan_mode == GNSS_WIFI) {
			//GNSS_WIFI scan mode. Try a WIFI scan if not enough sats were detected.
			LOG_INF("Not enough sats detected - nb:%u < %u - Performing WIFI scan...", n_sv_detected, MIN_NB_SV);
			scan_wifi(atcontext);
		} else {
			//GNSS only scanning mode.  Send the NOLOC message if not enough sats were detected.
			LOG_INF("Not enough sats detected - nb:%u < %u - Sending NOLOC message...", n_sv_detected, MIN_NB_SV);
			atcontext->uplink_type = NOLOC_T;
			at_event_send(EVENT_SEND_UPLINK);
		}
	} else {
		//enough sats detected, send the nav message.
		atcontext->gnss_scan_results.nb_sv = n_sv_detected;
		atcontext->gnss_scan_results.nav_msg_size = gnss_result.length;
		atcontext->gnss_scan_results.nav_msg = gnss_result.buffer;

		atcontext->uplink_type = GNSS_T;
		at_event_send(EVENT_SEND_UPLINK);
	}
}

void start_gnss_scan(at_ctx_t *app_ctx)
{

	atcontext = app_ctx;

	LOG_INF("Scanning GNSS...");

	struct sid_timespec curr_time;
	void * drv_ctx = lr11xx_get_drv_ctx();
	if (lr11xx_system_wakeup(drv_ctx) != LR11XX_STATUS_OK) {
		LOG_ERR("LR11XX wake-up fail");
		return;
	}

	sid_error_t ret = sid_get_time(atcontext->handle, SID_GET_GPS_TIME, &curr_time);
	if (SID_ERROR_NONE != ret) {
		LOG_ERR("sid_get_time fail %d", ret);
		return;
	}
	lr11xx_status_t status;
	status = lr11xx_gnss_set_assistance_position(drv_ctx, &assistance_position);
	if (status == LR11XX_STATUS_ERROR) {
		LOG_ERR("set assist-pos fail");
		return;
	}
	//Save the GPS capture time
	atcontext->gnss_scan_results.capture_time = (uint32_t) curr_time.tv_sec;

	status = lr11xx_gnss_scan_assisted(drv_ctx,
		curr_time.tv_sec,
		LR11XX_GNSS_OPTION_BEST_EFFORT,
		LR11XX_GNSS_RESULTS_DOPPLER_ENABLE_MASK | LR11XX_GNSS_RESULTS_DOPPLER_MASK | LR11XX_GNSS_RESULTS_BIT_CHANGE_MASK,
		NB_MAX_SV
	);
	if (status == LR11XX_STATUS_ERROR)
		LOG_ERR("GNSS: assisted scan fail");
#ifdef GNSS_DEBUG
	else
		LOG_INF("GNSS assisted scan started %u, %f %f", curr_time.tv_sec, assistance_position.latitude, assistance_position.longitude);
#endif /* GNSS_DEBUG */
}
