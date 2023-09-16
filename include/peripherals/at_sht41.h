// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef AT_SHT41_H
#define AT_SHT41_H

int init_at_sht41(void);
int get_temp_hum(struct at_sensors *sensors); 

#endif /* AT_SHT41_H */