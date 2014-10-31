/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
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
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include "opal/class/opal_pointer_array.h"
#include "opal/util/argv.h"
#include "opal/runtime/opal_info_support.h"

#include "orcm/include/orcm/frameworks.h"

#include "orcm/runtime/orcm_info_support.h"

const char *orcm_info_type_orcm = "orcm";

static bool orcm_info_registered = false;

void orcm_info_register_types(opal_pointer_array_t *mca_types)
{
    int i;

    /* add the top-level type */
    opal_pointer_array_add(mca_types, "orcm");

    /* push all the types found by autogen */
    for (i=0; NULL != orcm_frameworks[i]; i++) {
        opal_pointer_array_add(mca_types, orcm_frameworks[i]->framework_name);
    }
}

int orcm_info_register_framework_params(opal_pointer_array_t *component_map)
{
    int rc;

    if (orcm_info_registered) {
        return ORCM_SUCCESS;
    }

    orcm_info_registered = true;

    rc = opal_info_register_framework_params(component_map);
    if (OPAL_SUCCESS != rc) {
        return rc;
    }

    return opal_info_register_project_frameworks("orcm", orcm_frameworks, component_map);
}

void orcm_info_close_components(void)
{
    int i;

    for (i=0; NULL != orcm_frameworks[i]; i++) {
        (void) mca_base_framework_close(orcm_frameworks[i]);
    }
}

void orcm_info_show_orcm_version(const char *scope)
{
    char *tmp, *tmp2;

    asprintf(&tmp, "%s:version:full", orcm_info_type_orcm);
    tmp2 = opal_info_make_version_str(scope, 
                                      ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION, 
                                      ORCM_RELEASE_VERSION, 
                                      ORCM_GREEK_VERSION,
                                      ORCM_REPO_REV);
    opal_info_out("Open RCM", tmp, tmp2);
    free(tmp);
    free(tmp2);
    asprintf(&tmp, "%s:version:repo", orcm_info_type_orcm);
    opal_info_out("Open RCM repo revision", tmp, ORCM_REPO_REV);
    free(tmp);
    asprintf(&tmp, "%s:version:release_date", orcm_info_type_orcm);
    opal_info_out("Open RCM release date", tmp, ORCM_RELEASE_DATE);
    free(tmp);
}

