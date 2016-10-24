/*
 * Copyright (c) 2015-2016      Intel Corporation.  All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef CFGI_FILE30_H
#define CFGI_FILE30_H

#include "orcm/mca/cfgi/cfgi.h"

/*These define REGEX to validate data types*/
#define version_REGEX "^[[:space:]]*[[:digit:]]+(\\.[[:digit:]]+)?[[:space:]]*$"

ORCM_DECLSPEC extern orcm_cfgi_base_component_t mca_cfgi_file30_component;
ORCM_DECLSPEC extern orcm_cfgi_base_module_t orcm_cfgi_file30_module;
extern int fileId;
#endif /* CFGI_FILE30_H */
