/* -*- c -*-
 *
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#ifndef ORCM_CONFIG_H
#define ORCM_CONFIG_H

#include "orte_config.h"

#define ORCM_IDENT_STRING OPAL_IDENT_STRING

#  if OPAL_C_HAVE_VISIBILITY
#    define ORCM_DECLSPEC           __opal_attribute_visibility__("default")
#    define ORCM_MODULE_DECLSPEC    __opal_attribute_visibility__("default")
#  else
#    define ORCM_DECLSPEC
#    define ORCM_MODULE_DECLSPEC
#  endif

#endif  /* ORCM_CONFIG_H */
