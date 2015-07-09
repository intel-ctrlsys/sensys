/*
 * Copyright (c) 2015      Intel, Inc. All rights reserved
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
#ifndef ORCM_OCTL_GROUPING_H
#define ORCM_OCTL_GROUPING_H

#include "opal/class/opal_object.h"

BEGIN_C_DECLS

typedef struct {
    opal_list_item_t super;
    char * tag;
    char * nodename;
} orcm_logro_pair_t;
OBJ_CLASS_DECLARATION(orcm_logro_pair_t);

END_C_DECLS

#endif //ORCM_OCTL_GROUPING_H
