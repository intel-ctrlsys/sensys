/* -*- C -*-
 * 
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2009      Bull SAS.  All rights reserved.
 * Copyright (c) 2014      Intel, Inc.  All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 */
#ifndef NOTIFIER_FILE_H
#define NOTIFIER_FILE_H

#include "orcm_config.h"

#include "orcm/mca/notifier/notifier.h"

BEGIN_C_DECLS

typedef struct {
    orcm_notifier_base_component_t super;

    /* File name the traces should be sent to */
    char *fname;

    /* Priority */
    int priority;
} orcm_notifier_file_component_t;


/*
 * Notifier interfaces
 */

ORCM_MODULE_DECLSPEC extern orcm_notifier_file_component_t mca_notifier_file_component;
extern orcm_notifier_base_module_t orcm_notifier_file_module;

END_C_DECLS

#endif /* NOTIFIER_FILE_H */
