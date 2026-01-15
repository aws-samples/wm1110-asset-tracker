// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include <zephyr/kernel.h>
#include <zephyr/shell/shell.h>
#include "at_shell.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(at_shell, CONFIG_TRACKER_LOG_LEVEL);

#define AT_STACK_SIZE 5120
#define AT_WORKER_PRIO 5

struct k_work_q at_work_q;
K_THREAD_STACK_DEFINE(at_work_q_stack, AT_STACK_SIZE);

static at_ctx_t *atcontext;

static int cmd_enter_bootloader(const struct shell *sh, size_t argc, char **argv) {
	// see https://github.com/adafruit/Adafruit_nRF52_Bootloader/tree/7210c3914db0cf28e7b2c9850293817338259757#how-to-use
	NRF_POWER->GPREGRET = 0x57; // 0xA8 OTA, 0x4e Serial
	NVIC_SystemReset();
	return 0;
}

static int cmd_config_radio(const struct shell *sh, size_t argc, char **argv) {

	if (strlen(argv[1]) != 1) {
		shell_error(sh, "invalid radio type");
		return CMD_RETURN_ARGUMENT_INVALID;
	}

	switch (argv[1][0]) {
	case '1':
		atcontext->at_conf.sid_link_type = BLE_LM;
		shell_print(sh, "BLE radio type set for Sidewalk communications.");
		at_event_send(EVENT_RADIO_SWITCH);
		break;
	case '2':
		atcontext->at_conf.sid_link_type = LORA_LM;
		shell_print(sh, "LoRa radio type set for Sidewalk communications.");
		at_event_send(EVENT_RADIO_SWITCH);
		break;
	default:
		shell_error(sh, "radio type invalid. 1=ble, 2=lora");
		return CMD_RETURN_ARGUMENT_INVALID;
	}

	return 0;
}

static int cmd_trigger_scan(const struct shell *sh, size_t argc, char **argv) {
	shell_print(sh, "Triggering location scan...");
	at_event_send(EVENT_SCAN_LOC);
	return 0;
}

static int cmd_print_status(const struct shell *sh, size_t argc, char **argv) {
	shell_print(sh, "Sidewalk State: %d", atcontext->sidewalk_state);
	shell_print(sh, "Link Type: %s", 
		(atcontext->at_conf.sid_link_type == BLE_LM) ? "BLE" : 
		(atcontext->at_conf.sid_link_type == LORA_LM) ? "LoRa" : "Unknown");
	shell_print(sh, "Battery: %d%%", atcontext->sensors.batt);
	shell_print(sh, "Temperature: %.1f C", atcontext->sensors.temp);
	shell_print(sh, "Humidity: %.1f %%", atcontext->sensors.hum);
	return 0;
}

SHELL_STATIC_SUBCMD_SET_CREATE(
	sub_config, 
	SHELL_CMD_ARG(radio, NULL, "set sidewalk radio to use: 1=ble, 2=lora", cmd_config_radio, 2, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(asset_tracker,
	SHELL_CMD_ARG(status, NULL, "Print device status", cmd_print_status, 1, 0),
	SHELL_CMD_ARG(config, &sub_config, "Device config menu", NULL, 1, 0),
	SHELL_CMD_ARG(scan, NULL, "Trigger location scan", cmd_trigger_scan, 1, 0),
	SHELL_CMD_ARG(enter_bootloader, NULL, "Enter bootloader for UF2 flashing", cmd_enter_bootloader, 1, 0),
	SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(tracker, &asset_tracker, "Asset tracker commands", NULL);

void AT_CLI_init(at_ctx_t *at_context)
{
	atcontext = at_context;

	k_work_queue_init(&at_work_q);

	k_work_queue_start(&at_work_q, at_work_q_stack,
			   K_THREAD_STACK_SIZEOF(at_work_q_stack), AT_WORKER_PRIO, NULL);
}
