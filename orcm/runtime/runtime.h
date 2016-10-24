/*
 * Copyright (c) 2009-2010 Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2013-2014 Intel Corporation.  All rights reserved. 
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

/**
 * @file
 *
 * Interface into the ORCM Library
 */
#ifndef ORCM_RUNTIME_H
#define ORCM_RUNTIME_H

#include "orcm_config.h"

#include "orte/util/proc_info.h"

#include "orcm/runtime/orcm_globals.h"

BEGIN_C_DECLS

/**
 * Initialize the ORCM library
 *
 * This function should be called exactly once.  This function should
 * be called by every application using the ORCM library.
 *
 */
ORCM_DECLSPEC int orcm_init(orcm_proc_type_t flags);

/**
 * Finalize the ORCM library. Any function calling \code
 * openrcm_init should call \code openrcm_finalize. 
 *
 */
ORCM_DECLSPEC int orcm_finalize(void);

END_C_DECLS

#endif /* RUNTIME_H */
