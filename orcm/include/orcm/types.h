/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_TYPES_H
#define ORCM_TYPES_H

#include "orcm_config.h"

#include "orte/types.h"

/* Type defines for packing and unpacking */
#define    ORCM_ALLOC     (opal_data_type_t) (ORTE_DSS_ID_DYNAMIC + 1)
#define    ORCM_NODE      (opal_data_type_t) (ORTE_DSS_ID_DYNAMIC + 2)
#define    ORCM_RACK      (opal_data_type_t) (ORTE_DSS_ID_DYNAMIC + 3)
#define    ORCM_ROW       (opal_data_type_t) (ORTE_DSS_ID_DYNAMIC + 4)
#define    ORCM_CLUSTER   (opal_data_type_t) (ORTE_DSS_ID_DYNAMIC + 5)
#define    ORCM_SCHEDULER (opal_data_type_t) (ORTE_DSS_ID_DYNAMIC + 6)

#endif /* ORCM_TYPES_H */
