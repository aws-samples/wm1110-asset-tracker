// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef AT_TIMERS_H
#define AT_TIMERS_H

#include <zephyr/kernel.h>

void scan_timer_set_and_run(k_timeout_t delay);
void ble_conn_timer_set_and_run(void);
void ble_conn_timer_stop(void);
void btn_press_timer_set_and_run(void);
void sm_device_profile_timer_set_and_run(k_timeout_t delay);
void btn_press_timer_stop(void);

extern bool ble_timeout;

#endif /* AT_TIMERS_H */