/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef IPMI_PARSER_INTERFACE_H
#define IPMI_PARSER_INTERFACE_H

#include <stdlib.h>

#define MAX_STR_LEN 256

typedef struct ipmi_collector {
    char bmc_address[MAX_STR_LEN];
    char user[MAX_STR_LEN];
    char pass[MAX_STR_LEN];
    char aggregator[MAX_STR_LEN];
    char hostname[MAX_STR_LEN];
    int auth_method;
    int priv_level;
    int port;
    int channel;
} ipmi_collector;

extern bool load_ipmi_config_file(void);

extern void start_aggregator_count(void);

extern bool get_next_aggregator_name(char* aggregator);

extern bool get_bmc_info(char* hostname, ipmi_collector *ic);

#endif
