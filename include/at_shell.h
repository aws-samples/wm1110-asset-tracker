// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef AT_SHELL_H
#define AT_SHELL_H

#include <asset_tracker.h>

#define CMD_RETURN_OK 0
#define CMD_RETURN_HELP 1
#define CMD_RETURN_ARGUMENT_INVALID -EINVAL
#define CMD_RETURN_NOT_EXECUTED -ENOEXEC

#ifndef IN_RANGE
#define IN_RANGE(val, min, max) ((val >= min) && (val <= max))
#endif

void AT_CLI_init(at_ctx_t *at_context);

#endif /* AT_SHELL_H */
