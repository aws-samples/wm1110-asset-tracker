// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include "at_shell.h"
#include <halo_lr11xx_radio.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_shell, CONFIG_TRACKER_LOG_LEVEL);

#define AT_STACK_SIZE 5120
#define AT_WORKER_PRIO 5

#define RSSI_RANGE_MAX 90
#define RSSI_RANGE_MIN 20
//MAC str len for ff:ff:ff:ff:ff:ff format
#define MAC_STR_LEN 17
#define MAC_NUM_BYTES 6

struct k_work_q at_work_q;
K_THREAD_STACK_DEFINE(at_work_q_stack, AT_STACK_SIZE);

static at_ctx_t *atcontext;

DECLARE_RESULT(size_t, StrToHexRet, MAC_INVALID);
DECLARE_RESULT(uint8_t, HexToByteRet, NOT_HEX);
DECLARE_RESULT(uint8_t, StrToDecRet, OUT_RANGE);

static HexToByteRet hex_nib_to_byte(char hex)
{
	if (IN_RANGE(hex, '0', '9')) {
		return (HexToByteRet)Result_Val(hex - '0');
	}
	if (IN_RANGE(hex, 'A', 'F')) {
		return (HexToByteRet)Result_Val(hex - 'A' + 10);
	}
	if (IN_RANGE(hex, 'a', 'f')) {
		return (HexToByteRet)Result_Val(hex - 'a' + 10);
	}

	return (HexToByteRet)Result_Err(NOT_HEX);
}

static StrToHexRet convert_str_to_mac(uint8_t *out, size_t out_limit, char *in)
{
	char *working_ptr = in;

	if (strlen(in) != MAC_STR_LEN) {
		LOG_DBG("strlen = %d", strlen(in));
		return (StrToHexRet)Result_Err(MAC_INVALID);
	}

	uint8_t payload_size = 0;

	for (; payload_size < MAC_NUM_BYTES; working_ptr = working_ptr + 3) {
		HexToByteRet hi = hex_nib_to_byte(*working_ptr);
		HexToByteRet lo = hex_nib_to_byte(*(working_ptr + 1));

		if (hi.result == Err || lo.result == Err) {
			LOG_DBG("Not hex error = h:%d l:%d", hi.result, lo.result);
			return (StrToHexRet)Result_Err(MAC_INVALID);
		}
		out[payload_size] = (hi.val_or_err.val << 4) + lo.val_or_err.val;
		payload_size++;
	}

	return (StrToHexRet)Result_Val(payload_size);
}

static StrToDecRet rssi_string_to_dec(char *string)
{
	uint8_t value = 0;


	if ((*string != '-') || (strlen(string) > 3)) {
		LOG_DBG("rssi no dash or strl(%d) too long", strlen(string));
		return (StrToDecRet)Result_Err(OUT_RANGE);
	}
	string++; //skip the dash

	while (*string != '\0') {
		if (IN_RANGE(*string, '0', '9') == false) {
			return (StrToDecRet)Result_Err(OUT_RANGE);
		}
		value *= 10;
		value += (uint8_t)((*string) - '0');
		string++;
	}
	return (StrToDecRet)Result_Val(value);
}


static int cmd_enter_bootloader(const struct shell *sh, size_t argc, char **argv) {
	// see https://github.com/adafruit/Adafruit_nRF52_Bootloader/tree/7210c3914db0cf28e7b2c9850293817338259757#how-to-use
  	NRF_POWER->GPREGRET = 0x57; // 0xA8 OTA, 0x4e Serial
  	NVIC_SystemReset();
	return 0;
}


static int cmd_workshop_enable(const struct shell *sh, size_t argc, char **argv) {

	atcontext->at_conf.workshop_mode = false;  //force false to ensure it will be enabled by the event
	at_event_send(EVENT_SWITCH_WS_MODE);
	shell_warn(sh, "\n  Workshop mode enabled - Device will use the static mac and rssi values from workshop config and will uplink using BLE.\n");
	return 0;

}

static int cmd_workshop_disable(const struct shell *sh, size_t argc, char **argv) {

	atcontext->at_conf.workshop_mode = true;  //force true to ensure it will be disabled by the event
	at_event_send(EVENT_SWITCH_WS_MODE);
	shell_warn(sh, "\n  Workshop mode disabled - Device will free scan GNSS & WIFI and uplink using LoRa.\n");
	return 0;

}

static int cmd_workshop_set_mac(const struct shell *sh, size_t argc, char **argv) {

	uint8_t mac_index = 0;
	uint8_t mac[6];
	int8_t rssi;
	
	if (strlen(argv[1]) != 1) {
		shell_error(sh, "invalid MAC index");
		return CMD_RETURN_ARGUMENT_INVALID;
	}

	switch (argv[1][0]) {
	case '1':
		mac_index = 0;
		break;
	case '2':
		mac_index = 1;
		break;
	default:
		shell_error(sh, "MAC index invalid. Please use 1 or 2");
		return CMD_RETURN_ARGUMENT_INVALID;
	}

	StrToHexRet ret_mac = convert_str_to_mac(mac, sizeof(mac), argv[2]);

	if (ret_mac.result == Err) {
		switch (ret_mac.val_or_err.err) {
		case MAC_INVALID:
			shell_error(sh, "MAC string invalid.  ex:  FF:FF:FF:FF:FF:FF");
			return CMD_RETURN_ARGUMENT_INVALID;
		default:
			// shell_error(sh, "MAC string invalid.  ex:  FF:FF:FF:FF:FF:FF or 11 22 33 44 55 66");
			return CMD_RETURN_HELP;
		}
	}

	StrToDecRet ret_rssi = rssi_string_to_dec(argv[3]);

	if (ret_rssi.result == Err) {
		switch (ret_rssi.val_or_err.err) {
		case OUT_RANGE:
			shell_error(sh, "RSSI value invalid.  ex: -66");
			return CMD_RETURN_ARGUMENT_INVALID;
		default:
			// shell_error(sh, "RSSI value invalid.  ex: -66");
			return CMD_RETURN_HELP;
		}
	}

	if ((ret_rssi.val_or_err.val < RSSI_RANGE_MIN) || (ret_rssi.val_or_err.val > RSSI_RANGE_MAX)) {
		shell_error(sh,	"RSSI value out of range: -%d to -%d", RSSI_RANGE_MIN, RSSI_RANGE_MAX);
		return CMD_RETURN_ARGUMENT_INVALID;
	}

	//convert result to negative signed byte
	rssi = (int8_t) (0 - ret_rssi.val_or_err.val);

	//set the values in the app
	atcontext->at_conf.workshop_wifi_macs[mac_index][0] = mac[0];
	atcontext->at_conf.workshop_wifi_macs[mac_index][1] = mac[1];
	atcontext->at_conf.workshop_wifi_macs[mac_index][2] = mac[2];
	atcontext->at_conf.workshop_wifi_macs[mac_index][3] = mac[3];
	atcontext->at_conf.workshop_wifi_macs[mac_index][4] = mac[4];
	atcontext->at_conf.workshop_wifi_macs[mac_index][5] = mac[5];

	atcontext->at_conf.workshop_wifi_rssi[mac_index] = rssi;	

	shell_print(sh, "Static workshop MAC %d and RSSI set!", mac_index+1);
	
	return 0;

}

static int cmd_print_workshop_status(const struct shell *sh, size_t argc, char **argv) {

	const uint8_t *mac1 = atcontext->at_conf.workshop_wifi_macs[0];
	const uint8_t *mac2 = atcontext->at_conf.workshop_wifi_macs[1];
	const int8_t rssi1 = atcontext->at_conf.workshop_wifi_rssi[0];
	const int8_t rssi2 = atcontext->at_conf.workshop_wifi_rssi[1];

	shell_print(sh, "Workshop Mode:  %s", (atcontext->at_conf.workshop_mode == true) ? "ENABLED" : "DISABLED");
	shell_print(sh, "        MAC 1:  %02x:%02x:%02x:%02x:%02x:%02x  - RSSI: %d dBm", mac1[0], mac1[1], mac1[2], mac1[3], mac1[4], mac1[5], rssi1 );
	shell_print(sh, "        MAC 2:  %02x:%02x:%02x:%02x:%02x:%02x  - RSSI: %d dBm", mac2[0], mac2[1], mac2[2], mac2[3], mac2[4], mac2[5], rssi2 );
	return 0;

}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_config, 
	SHELL_CMD_ARG(dummy, NULL, "Coming soon!", cmd_workshop_enable, 1, 0),
	// SHELL_CMD_ARG(disable, NULL, "disable workshop mode - devices scans and uplinks based on tracker config", cmd_workshop_disable, 1, 0),
	// SHELL_CMD_ARG(mac, NULL, "<1|2> + <bssid> + <rssi> - set static mac and rssi used in workshop mode\n"
	// 						 "\t        ex: workshop mac 1 04:18:D6:36:F1:EC -55\n", 
	// 	cmd_workshop_set_mac, 4, 0),
	// SHELL_CMD_ARG(status, NULL, "print workshop mode status and static mac/rssi config", cmd_print_workshop_status, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_workshop, 
	SHELL_CMD_ARG(status, NULL, "print workshop mode status and static mac/rssi config", cmd_print_workshop_status, 1, 0),
	SHELL_CMD_ARG(enable, NULL, "enables workshop mode - devices sends static wifi macs using BLE Sidewalk link", cmd_workshop_enable, 1, 0),
	SHELL_CMD_ARG(disable, NULL, "disable workshop mode - devices scans and uplinks based on tracker config", cmd_workshop_disable, 1, 0),
	SHELL_CMD_ARG(mac, NULL, "<1|2> + <bssid> + <rssi> - set static mac and rssi used in workshop mode\n"
							 "\t        ex: tracker workshop mac 1 04:18:D6:36:F1:EC -55\n", 
		cmd_workshop_set_mac, 4, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(asset_tracker,
	SHELL_CMD_ARG(info, NULL, "Print Sidewalk Asset Tracker app info",	NULL, 1, 0),
	SHELL_CMD_ARG(status, NULL, "Print device status",	NULL, 1, 0),
	SHELL_CMD_ARG(config, &sub_config, "Device config menu",	NULL, 1, 0),
	// SHELL_CMD_ARG(scan_wifi, NULL, "Scan Wifi and print results",	NULL, 1, 0),
	// SHELL_CMD_ARG(scan_gnss, NULL, "Scan GNSS and print results",	NULL, 1, 0),
	SHELL_CMD_ARG(workshop, &sub_workshop, "Workshop mode menu",	NULL, 1, 0),
	SHELL_CMD_ARG(factory_reset, NULL, "Factory reset device - wipe storage and Sidewalk session",	NULL, 2, 0),
	SHELL_CMD_ARG(enter_bootloader, NULL, "Enter bootloader for UF2 flashing", cmd_enter_bootloader, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(tracker, &asset_tracker, "Asset tracker commands", NULL);

void AT_CLI_init(at_ctx_t *at_context)
{
	// sidewalk_handle = handle;
    // sidewalk_config = config;
	atcontext = at_context;

	k_work_queue_init(&at_work_q);

	k_work_queue_start(&at_work_q, at_work_q_stack,
			   K_THREAD_STACK_SIZEOF(at_work_q_stack), AT_WORKER_PRIO, NULL);
}
