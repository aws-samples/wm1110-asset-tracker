// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef AT_SHELL_H
#define AT_SHELL_H

#include <asset_tracker.h>

#define CMD_RETURN_OK 0
#define CMD_RETURN_HELP 1
#define CMD_RETURN_ARGUMENT_INVALID -EINVAL
#define CMD_RETURN_NOT_EXECUTED -ENOEXEC

typedef enum { Val, Err } result_t;

#define Result(value_t, error_e)                                                                   \
	struct {                                                                                   \
		result_t result;                                                                   \
		union {                                                                            \
			value_t val;                                                               \
			error_e err;                                                               \
		} val_or_err;                                                                      \
	}

#define Result_Val(value)                                                                          \
	{                                                                                          \
		.result = Val, .val_or_err.val = value                                             \
	}
#define Result_Err(error_code)                                                                     \
	{                                                                                          \
		.result = Err, .val_or_err.err = error_code                                        \
	}

#define DECLARE_RESULT(value_t, name, error_cases...)                                              \
	enum name##_error_cases{ error_cases };                                                    \
	typedef Result(value_t, enum name##_error_cases) name;


void send_scan_result(at_ctx_t *at_context);
void start_gnss_scan(at_ctx_t *at_ctx);
void app_event_wifi_scan(void);

void AT_CLI_init(at_ctx_t *at_context);
#endif /* AT_SHELL_H */