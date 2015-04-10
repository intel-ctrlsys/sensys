/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_DB_UTILS_H
#define ORCM_DB_UTILS_H

#include "opal/dss/dss_types.h"

typedef enum {
    ORCM_DB_ITEM_INTEGER,
    ORCM_DB_ITEM_REAL,
    ORCM_DB_ITEM_STRING
} orcm_db_item_type_t;

typedef struct {
    orcm_db_item_type_t item_type;
    opal_data_type_t opal_type;
    union {
        long long int value_int;
        double value_real;
        char *value_str;
    } value;
} orcm_db_item_t;

int opal_value_to_orcm_db_item(const opal_value_t *kv,
                               orcm_db_item_t *item);


#endif /* ORCM_DB_UTILS_H */
