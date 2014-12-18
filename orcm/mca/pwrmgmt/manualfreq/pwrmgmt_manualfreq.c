/*
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"
#include "orcm/types.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#include <stdio.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_DIRENT_H
#include <dirent.h>
#endif  /* HAVE_DIRENT_H */
#include <ctype.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libudev.h>

#include "opal_stdint.h"
#include "opal/class/opal_list.h"
#include "opal/dss/dss.h"
#include "opal/util/os_path.h"
#include "opal/util/output.h"
#include "opal/util/os_dirpath.h"

#include "orte/util/name_fns.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/routed/routed.h"
#include "orte/mca/grpcomm/grpcomm.h"
#include "orte/mca/grpcomm/base/base.h"
#include "orte/runtime/orte_wait.h"
#include "orte/util/regex.h"

#include "orcm/mca/db/db.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/cfgi/cfgi_types.h"
#include "orcm/mca/scd/base/base.h"
#include "orcm/mca/pwrmgmt/base/base.h"
#include "pwrmgmt_manualfreq.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void alloc_notify(orcm_alloc_t* alloc);
static void dealloc_notify(orcm_alloc_t* alloc);
static int set_attributes(orcm_session_id_t session, opal_list_t* attr);
static int reset_attributes(orcm_session_id_t session, opal_list_t* attr);
static int get_attributes(orcm_session_id_t session, opal_list_t* attr);
static int get_current_power(orcm_session_id_t session, double* power);

/* instantiate the module */
orcm_pwrmgmt_base_API_module_t orcm_pwrmgmt_manualfreq_module = {
    init,
    finalize,
    alloc_notify,
    dealloc_notify,
    set_attributes,
    reset_attributes,
    get_attributes,
    get_current_power
};

static int init(void)
{
    return ORCM_SUCCESS;
}

static void finalize(void)
{
}

static int set_attributes(orcm_session_id_t session, opal_list_t* attr) 
{
    return OPAL_SUCCESS;
}

static int reset_attributes(orcm_session_id_t session, opal_list_t* attr) 
{
    return OPAL_SUCCESS;
}

static int get_attributes(orcm_session_id_t session, opal_list_t* attr) 
{

    return OPAL_SUCCESS;
}

static int get_current_power(orcm_session_id_t session, double* power) 
{
    *power = -1.0;

    return ORTE_ERR_NOT_SUPPORTED;
}

void alloc_notify(orcm_alloc_t* alloc)
{
}

void dealloc_notify(orcm_alloc_t* alloc)
{
}

int orcm_pwrmgmt_base_set_attributes(orcm_session_id_t session, opal_list_t* attr)
{
    return OPAL_SUCCESS;
}

int orcm_pwrmgmt_base_reset_attributes(orcm_session_id_t session, opal_list_t* attr)
{
    return OPAL_SUCCESS;
}

int orcm_pwrmgmt_base_get_attributes(orcm_session_id_t session, opal_list_t* attr)
{
    return OPAL_SUCCESS;
}
