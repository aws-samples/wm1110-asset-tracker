// Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
// SPDX-License-Identifier: MIT-0

#include <stdint.h>
#include <stddef.h>

extern const char *const build_time_stamp;

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
    LOG_PRINTK("App Version         = v0.9\n");                                                       \
    LOG_PRINTK("Build Time          = %s\n", build_time_stamp);                                        \
    LOG_PRINTK("---------------------------------------------------------------------------------\n");

#define PRINT_LR_VERSION()                                                                             \
    LOG_PRINTK("LR11XX Type         = 0x%02X\n", version_trx.type );                                   \
    LOG_PRINTK("       HW Version   = 0x%02X\n", version_trx.hw );                                     \
    LOG_PRINTK("       FW Version   = 0x%02X\n", version_trx.fw );                                     \
    LOG_PRINTK("---------------------------------------------------------------------------------\n");

#else
#define PRINT_AWSIOT_LOGO()
#define PRINT_AT_VERSION()
#endif /* CONFIG_LOG */