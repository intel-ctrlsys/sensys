/* Copyright (c) 2016      Intel Corporation. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "opal/mca/mca.h"
#include "opal/util/argv.h"
#include "opal/util/fd.h"
#include "opal/util/output.h"
#include "opal/mca/base/base.h"
#include "opal/class/opal_pointer_array.h"
#include "opal/runtime/opal_progress_threads.h"


#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/cfgi/base/base.h"
#include "orcm/mca/parser/base/base.h"

/*
 * The following file was created by configure.  It contains extern
 * statements and the definition of an array of pointers to each
 * component's public mca_base_component_t struct.
 */

#include "orcm/mca/parser/base/static-components.h"

/*
 * Global variables
 */

orcm_parser_API_module_t orcm_parser = {
        orcm_parser_base_open_file,
        orcm_parser_base_close_file,
        orcm_parser_base_retrieve_document,
        orcm_parser_base_retrieve_section,
        orcm_parser_base_retrieve_section_from_list,
        orcm_parser_base_write_section
};

orcm_parser_base_t orcm_parser_base;

static int orcm_parser_base_close(void)
{
    orcm_parser_active_module_t *active_module;

    OPAL_LIST_FOREACH(active_module, &orcm_parser_base.actives, orcm_parser_active_module_t) {
        if (NULL != active_module->module->finalize) {
            active_module->module->finalize();
        }
    }

    OPAL_LIST_DESTRUCT(&orcm_parser_base.actives);

    /* Close all remaining available components */
    return mca_base_framework_components_close(&orcm_parser_base_framework, NULL);
}

/**
 * Function for finding and opening either all MCA components, or the one
 * that was specifically requested via a MCA parameter.
 */
static int orcm_parser_base_open(mca_base_open_flag_t flags)
{
    int rc;

    /* construct the array of active modules */
    OBJ_CONSTRUCT(&orcm_parser_base.actives, opal_list_t);

    /* Open up all available components */
    rc = mca_base_framework_components_open(&orcm_parser_base_framework, flags);
    return rc;
}

MCA_BASE_FRAMEWORK_DECLARE(orcm, parser, "ORCM PARSER FRAMEWORK", NULL,
                           orcm_parser_base_open, orcm_parser_base_close,
                           mca_parser_base_static_components, 0);


/* framework class instanstiations */
OBJ_CLASS_INSTANCE(orcm_parser_active_module_t,
                   opal_list_item_t,
                   NULL, NULL);
