/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef MRNETSCON_H
#define MRNETSCON_H

#include "orte_config.h"
#include "orte/types.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "opal/dss/dss_types.h"
#include "opal/class/opal_pointer_array.h"
#include "orte/mca/rml/rml_types.h"

BEGIN_C_DECLS

/* Initialize the MRNETSCON library */
ORTE_DECLSPEC int mrnetscon_init(void);

ORTE_DECLSPEC void mrnetscon_finalize(void);

END_C_DECLS

#endif /* MRNETSCON_H */
