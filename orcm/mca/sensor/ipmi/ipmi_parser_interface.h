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

typedef struct ipmi_collector {
    char bmc_address[256];
    char user[256];
    char pass[256];
    char aggregator[256];
    char hostname[256];
    int auth_method;
    int priv_level;
    int port;
    int channel;
} ipmi_collector;

extern void load_ipmi_config_file(void);

extern void start_aggregator_count(void);

extern bool get_next_aggregator_name(char* aggregator);

extern bool get_bmc_info(char* hostname, ipmi_collector *ic);

#endif
