/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/**
 * @file
 *
 * IPMI resource manager sensor 
 */
#ifndef ORCM_SENSOR_HWLOC_DECLS_H
#define ORCM_SENSOR_HWLOC_DECLS_H

#define MAX_INVENTORY_KEYWORDS      7
#define MAX_INVENTORY_SUB_KEYWORDS  5
#define MAX_INVENTORY_KEYWORD_SIZE  30

/* Increment the MAX_INVENTORY_KEYWORDS size with every new addition here */
static char inv_keywords[MAX_INVENTORY_KEYWORDS][MAX_INVENTORY_SUB_KEYWORDS][MAX_INVENTORY_KEYWORD_SIZE] =
    {
     {"bios","version","","","bios_version"},
     {"bios","date","","","bios_release_date"},
     {"bios","vendor","","","bios_vendor"},
     {"cpu","vendor","","","cpu_vendor"},
     {"cpu","family","","","cpu_family"},
     {"cpu","model","","number","cpu_model"},
     {"cpu","model","number","","cpu_model_number"},
     };
#endif
