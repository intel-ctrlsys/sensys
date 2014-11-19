/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"

#include "orcm/util/attr.h"

char* orcm_attr_key_print(orte_attribute_key_t key)
{
    if (ORCM_ATTR_KEY_BASE < key &&
        key < ORCM_ATTR_KEY_MAX) {
        /* belongs to ORCM, so we handle it */
        switch(key) {

        default:
            return "UNKNOWN-KEY";
        }
    }

    /* get here if nobody know what to do */
    return "UNKNOWN-KEY";
}
