/*!
 * @file      alamanac_update.c
 *
 * @brief     full almanac update of LR11xx
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

#ifdef ALMANAC_UPDATE
#include <time.h>
#include <zephyr/logging/log.h>

#include <lr1110/almanac_update.h>
#include <lr1110/almanac.h> // generated by get_full_almanac.py

#include <lr11xx_hal.h>
#include "halo_lr11xx_radio.h"

#define OFFSET_BETWEEN_GPS_EPOCH_AND_UNIX_EPOCH 315964800

#define TIME_BUFFER_SIZE 80

LOG_MODULE_REGISTER(almanac, CONFIG_TRACKER_LOG_LEVEL);

static bool get_almanac_crc( const void* ral_context, uint32_t* almanac_crc )
{
    lr11xx_status_t                         err;
    lr11xx_gnss_context_status_bytestream_t context_status_bytestream;
    lr11xx_gnss_context_status_t            context_status;

    err = lr11xx_gnss_get_context_status( ral_context, context_status_bytestream );
    if( err != LR11XX_STATUS_OK )
    {
        LOG_ERR( "Failed to get gnss context status" );
        return false;
    }

    err = lr11xx_gnss_parse_context_status_buffer( context_status_bytestream, &context_status );
    if( err != LR11XX_STATUS_OK )
    {
        LOG_ERR( "Failed to parse gnss context status to get almanac status" );
        return false;
    }

    *almanac_crc = context_status.global_almanac_crc;

    return true;
}

static bool _almanac_update( const void* ral_context )
{
    uint32_t global_almanac_crc, local_almanac_crc;
    local_almanac_crc =
        ( full_almanac[6] << 24 ) + ( full_almanac[5] << 16 ) + ( full_almanac[4] << 8 ) + ( full_almanac[3] );

    if( get_almanac_crc( ral_context, &global_almanac_crc ) == false )
    {
        LOG_ERR( "Failed to get almanac CRC before update" );
        return false;
    }
    if( global_almanac_crc != local_almanac_crc )
    {
        LOG_INF( "Local almanac doesn't match LR11XX almanac -> start update" );

        /* Load almanac in flash */
        uint16_t almanac_idx = 0;
        while( almanac_idx < sizeof( full_almanac ) )
        {
            if( lr11xx_gnss_almanac_update( ral_context, full_almanac + almanac_idx, 1 ) != LR11XX_STATUS_OK )
            {
                LOG_ERR( "Failed to update almanac" );
                return false;
            }
            almanac_idx += LR11XX_GNSS_SINGLE_ALMANAC_WRITE_SIZE;
        }

        /* Check CRC again to confirm proper update */
        if( get_almanac_crc( ral_context, &global_almanac_crc ) == false )
        {
            LOG_ERR( "Failed to get almanac CRC after update" );
            return false;
        }
        if( global_almanac_crc != local_almanac_crc )
        {
            LOG_ERR( "Local almanac doesn't match LR11XX almanac -> update failed" );
            return false;
        }
        else
        {
            LOG_INF( "Almanac update succeeded" );
        }
    }
    else
    {
        LOG_INF( "Local almanac matches LR11XX almanac -> no update" );
    }

    return true;
}

int almanac_update()
{
    /* Convert raw almanac date to epoch time */
    uint16_t almanac_date_raw = ( uint16_t )( ( full_almanac[2] << 8 ) | full_almanac[1] );
    time_t   almanac_date = ( OFFSET_BETWEEN_GPS_EPOCH_AND_UNIX_EPOCH + 24 * 3600 * ( 2048 * 7 + almanac_date_raw ) );

    /* Convert epoch time to human readbale format */
    char             buf[TIME_BUFFER_SIZE];
    const struct tm* time = localtime( &almanac_date );
    strftime( buf, TIME_BUFFER_SIZE, "%a %Y-%m-%d %H:%M:%S %Z", time );
    LOG_INF( "Source almanac date: %s", buf );

	void* drv_ctx = lr11xx_get_drv_ctx();
	if (lr11xx_system_wakeup(drv_ctx) != LR11XX_STATUS_OK) {
		LOG_ERR("wakeup fail");
		return LR11XX_STATUS_ERROR;
	}

	uint16_t chip_almanac_age;
	lr11xx_status_t ret = lr11xx_gnss_get_almanac_age_for_satellite(drv_ctx, 0, &chip_almanac_age);
	if (ret != LR11XX_STATUS_OK) {
		LOG_ERR("lr1110_gnss_get_almanac_age_for_satellite() fail");
		return ret;
	}
	LOG_INF("chip_almanac_age 0x%x, header raw 0x%x", chip_almanac_age, almanac_date_raw);

	_almanac_update(drv_ctx);
	
	return 0;
}
#endif /* ALMANAC_UPDATE */