/*
 * Local override for LR1110 Sub-GHz link configuration
 * This file overrides the default config in the Sidewalk SDK
 * 
 * Key changes from SDK defaults:
 *   - Max TX power increased from 20dBm to 22dBm (hardware max)
 *   - Antenna gain set to -1dBi (Wio Tracker 1110 onboard antenna)
 * 
 * The Wio Tracker 1110 has a low-gain onboard antenna (-1dBi), so we need
 * the full +22dBm TX power to compensate and achieve adequate link budget.
 */

#include <app_subGHz_config.h>

/* Hardware maximum TX power for LR1110 */
#define RADIO_MAX_TX_POWER_WIO 22

/* Antenna gain macro: converts dBi to internal format (dBi * 100) */
#define RADIO_ANT_GAIN(X) ((X) * 100)

/* Wio Tracker 1110 onboard antenna gain: -1 dBi */
#define WIO_ANTENNA_GAIN_DBI (-1)

static struct sid_sub_ghz_links_config wio_sub_ghz_link_config = {
    .enable_link_metrics = true,
    .metrics_msg_retries = 3,
    .sar_dcr = 100,
    .registration_config = {
        .enable = true,
        .periodicity_s = UINT32_MAX,
    },
    /* Use full +22dBm TX power for both FSK (link2) and LoRa (link3) */
    .link2_max_tx_power_in_dbm = RADIO_MAX_TX_POWER_WIO,
    .link3_max_tx_power_in_dbm = RADIO_MAX_TX_POWER_WIO,
};

/*
 * Override the weak function from Sidewalk SDK
 * Returns our custom config with +22dBm max TX power
 */
struct sid_sub_ghz_links_config *app_get_sub_ghz_config(void)
{
    return &wio_sub_ghz_link_config;
}
