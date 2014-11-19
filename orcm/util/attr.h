/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_ATTRS_H
#define ORCM_ATTRS_H

#include "orcm_config.h"
#include "orte/types.h"
#include "orte/util/attr.h"

/* define the mininum value of the ORCM keys just in
 * case someone someday puts a layer underneath us */
#define ORCM_ATTR_KEY_BASE        ORTE_ATTR_KEY_MAX


/* define a max value for ORCM keys */
#define ORCM_ATTR_KEY_MAX         ORCM_ATTR_KEY_BASE+1000


/* provide a handler for printing ORCM-level attr keys */
ORCM_DECLSPEC char* orcm_attr_key_print(orte_attribute_key_t key);

#endif
