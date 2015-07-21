/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 */

#ifndef MCA_ANALYTICS_BASE_H
#define MCA_ANALYTICS_BASE_H

/*
 * includes
 */
#include "orcm_config.h"

#include "opal/mca/mca.h"
#include "opal/mca/event/event.h"
#include "opal/dss/dss_types.h"
#include "opal/util/output.h"

#include "orcm/mca/analytics/analytics.h"


BEGIN_C_DECLS

/*
 * MCA framework
 */
ORCM_DECLSPEC extern mca_base_framework_t orcm_analytics_base_framework;


END_C_DECLS
#endif
