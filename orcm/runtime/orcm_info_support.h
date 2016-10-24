/*
 * Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2011 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2006-2012 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2010-2013 Los Alamos National Security, LLC.
 *                         All rights reserved.
 * Copyright (c) 2011-2012 University of Houston. All rights reserved.
 * Copyright (c) 2014      Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_INFO_REGISTER_H
#define ORCM_INFO_REGISTER_H

#include "orcm_config.h"

#include "opal/class/opal_pointer_array.h"

BEGIN_C_DECLS

ORCM_DECLSPEC extern const char *orcm_info_type_orcm;

ORCM_DECLSPEC void orcm_info_register_types(opal_pointer_array_t *mca_types);

ORCM_DECLSPEC int orcm_info_register_framework_params(opal_pointer_array_t *component_map);

ORCM_DECLSPEC void orcm_info_close_components(void);

ORCM_DECLSPEC void orcm_info_show_orcm_version(const char *scope);

END_C_DECLS

#endif
