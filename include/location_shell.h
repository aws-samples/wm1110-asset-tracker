/*
 * Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * SPDX-License-Identifier: MIT-0
 * 
 * Location shell commands for WM1110 Asset Tracker
 * Adapted from Nordic sid_end_device sample
 */

#ifndef LOCATION_SHELL_H
#define LOCATION_SHELL_H

#include <stdint.h>
#include <zephyr/shell/shell.h>
#include <asset_tracker.h>

/* Location Command Descriptions */
#define CMD_LOCATION_INIT_DESCRIPTION "Initialize sidewalk location library"

#define CMD_LOCATION_DEINIT_DESCRIPTION "Deinitialize sidewalk location library"

#define CMD_LOCATION_SEND_DESCRIPTION                                                              \
	"<LOCATION_LEVEL>\n"                                                                       \
	"Send a location uplink at the specified level 1-4.\n"                                     \
	"No level = automatic scaling mode (lowest power first)\n"                                 \
	"1 - BLE Uplink using Sidewalk Network location\n"                                         \
	"    (temporarily switches to BLE-only mode)\n"                                            \
	"2 - LoRa Triangulation (not supported)\n"                                                 \
	"3 - WiFi scan\n"                                                                          \
	"4 - GNSS"

#define CMD_LOCATION_SCAN_DESCRIPTION                                                              \
	"<LOCATION_LEVEL>\n"                                                                       \
	"Scan only (no send) at the specified level 1-4.\n"                                        \
	"No level = automatic scaling mode\n"                                                      \
	"1 - BLE (no scan, use 'location send 1' instead)\n"                                       \
	"2 - LoRa Triangulation (not supported)\n"                                                 \
	"3 - WiFi scan\n"                                                                          \
	"4 - GNSS"

#define CMD_LOCATION_STATUS_DESCRIPTION "Show location service status"

/* Argument counts */
#define CMD_LOCATION_INIT_ARG_REQUIRED 1
#define CMD_LOCATION_INIT_ARG_OPTIONAL 0

#define CMD_LOCATION_DEINIT_ARG_REQUIRED 1
#define CMD_LOCATION_DEINIT_ARG_OPTIONAL 0

#define CMD_LOCATION_SEND_ARG_REQUIRED 1
#define CMD_LOCATION_SEND_ARG_OPTIONAL 1

#define CMD_LOCATION_SCAN_ARG_REQUIRED 1
#define CMD_LOCATION_SCAN_ARG_OPTIONAL 1

#define CMD_LOCATION_STATUS_ARG_REQUIRED 1
#define CMD_LOCATION_STATUS_ARG_OPTIONAL 0

/* Initialize location shell with asset tracker context */
void location_shell_init(at_ctx_t *ctx);

/* Trigger BLE L1 location after stack is ready in BLE-only mode */
void location_shell_trigger_ble_location(void);

/* Shell command handlers */
int cmd_location_init(const struct shell *shell, int32_t argc, const char **argv);
int cmd_location_deinit(const struct shell *shell, int32_t argc, const char **argv);
int cmd_location_send(const struct shell *shell, int32_t argc, const char **argv);
int cmd_location_scan(const struct shell *shell, int32_t argc, const char **argv);
int cmd_location_status(const struct shell *shell, int32_t argc, const char **argv);

#endif /* LOCATION_SHELL_H */
