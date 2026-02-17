/*
 * Local override for LR1110 GNSS/WiFi location configuration
 * This file overrides the default config in the Sidewalk SDK
 */

#include <app_location_lr11xx_config.h>
#include <lr11xx_gnss_wifi_config.h>
#include <smtc_modem_geolocation_api.h>

/*
 * GNSS Scan Mode Options:
 *   SMTC_MODEM_GNSS_MODE_MOBILE - Multiple quick scans (~3-5 sec each), for moving objects
 *   SMTC_MODEM_GNSS_MODE_STATIC - Longer single scan (~30+ sec), better accuracy
 *
 * Constellation Options:
 *   SMTC_MODEM_GNSS_CONSTELLATION_GPS        - GPS only
 *   SMTC_MODEM_GNSS_CONSTELLATION_BEIDOU     - BeiDou only
 *   SMTC_MODEM_GNSS_CONSTELLATION_GPS_BEIDOU - Both (more satellites, better coverage)
 *
 * To change scan mode, set CONFIG_GNSS_SCAN_MODE_STATIC=y in prj.conf
 */

#ifdef CONFIG_GNSS_SCAN_MODE_STATIC
#define GNSS_SCAN_MODE SMTC_MODEM_GNSS_MODE_STATIC
#else
#define GNSS_SCAN_MODE SMTC_MODEM_GNSS_MODE_MOBILE
#endif

static lr11xx_gnss_wifi_config_t gnss_wifi_config = {
	.constellation_type = SMTC_MODEM_GNSS_CONSTELLATION_GPS_BEIDOU,
	.scan_mode = GNSS_SCAN_MODE
};

const lr11xx_gnss_wifi_config_t *get_location_cfg(void)
{
	return &gnss_wifi_config;
}
