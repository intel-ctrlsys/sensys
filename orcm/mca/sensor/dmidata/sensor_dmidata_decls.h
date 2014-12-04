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
/* NOTE: key for populating the lookup array
 * Each search item for a value contains 5 columns
 * The columns 1,2 and 3 are the key words for finding an inventory item
 * The column 4 contains the key word for enabling an ignore case
 * The column 5 contains a key word under which the corresponding inventory data is logged.
 */ 
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
