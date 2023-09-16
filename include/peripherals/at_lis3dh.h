// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef AT_LIS3DHTR_H
#define AT_LIS3DHTR_H

int init_at_lis3dh(void);
int get_accel(struct at_sensors *sensors);

#endif /* AT_LIS3DHTR_H */