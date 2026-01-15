// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#ifndef ASSET_TRACKER_VERSION_H
#define ASSET_TRACKER_VERSION_H

#include <stdint.h>
#include <stddef.h>

#if defined(CONFIG_LOG)
#define PRINT_AWSIOT_LOGO()                                                                            \
    LOG_PRINTK("          ↑↑↑                                                                    \n"); \
    LOG_PRINTK("       ↑↑↑   ↑↑↑                                                                 \n"); \
    LOG_PRINTK("    ↑↑↑   ↑↑↑   ↑↑↑          ↑↑    ↑↑   ↑   ↑↑    ↑↑↑        ↑↑          ↑↑↑↑↑↑↑↑\n"); \
    LOG_PRINTK("   ↑↑      ↑      ↑↑        ↑↑↑↑   ↑↑  ↑↑↑  ↑↑ ↑↑↑  ↑↑↑      ↑↑             ↑↑   \n"); \
    LOG_PRINTK(" ↑↑  ↑↑↑↑↑ ↑ ↑↑↑↑↑  ↑↑     ↑↑ ↑↑   ↑↑  ↑↑↑  ↑↑  ↑↑↑          ↑↑    ↑↑↑      ↑↑   \n"); \
    LOG_PRINTK("   ↑↑    ↑ ↑ ↑    ↑↑      ↑↑↑  ↑↑   ↑↑↑↑ ↑↑↑↑      ↑↑↑↑      ↑↑  ↑↑   ↑↑    ↑↑   \n"); \
    LOG_PRINTK("    ↑↑↑  ↑ ↑ ↑  ↑↑↑      ↑↑↑↑↑↑↑↑↑  ↑↑↑   ↑↑↑  ↑↑↑   ↑↑      ↑↑  ↑↑   ↑↑    ↑↑   \n"); \
    LOG_PRINTK("      ↑↑↑↑↑↑↑↑↑↑↑        ↑↑     ↑↑   ↑↑   ↑↑     ↑↑↑↑        ↑↑    ↑↑↑      ↑↑   \n"); \
    LOG_PRINTK("         ↑ ↑ ↑                                                                   \n")

#define PRINT_AT_VERSION()                                                                             \
    LOG_PRINTK("=================================================================================\n"); \
    LOG_PRINTK("||                      Sidewalk Asset Tracker Example                         ||\n"); \
    LOG_PRINTK("=================================================================================\n"); \
    LOG_PRINTK("App Version         = v1.0 (NCS v3.2.1)\n");                                          \
    LOG_PRINTK("Build Time          = %s %s\n", __DATE__, __TIME__);                                   \
    LOG_PRINTK("---------------------------------------------------------------------------------\n");

#define PRINT_LR_VERSION()                                                                             \
    LOG_PRINTK("LR11XX Type         = 0x%02X\n", version_trx.type );                                   \
    LOG_PRINTK("       HW Version   = 0x%02X\n", version_trx.hw );                                     \
    LOG_PRINTK("       FW Version   = 0x%02X\n", version_trx.fw );                                     \
    LOG_PRINTK("---------------------------------------------------------------------------------\n");

#else
#define PRINT_AWSIOT_LOGO()
#define PRINT_AT_VERSION()
#define PRINT_LR_VERSION()
#endif /* CONFIG_LOG */

#endif /* ASSET_TRACKER_VERSION_H */
