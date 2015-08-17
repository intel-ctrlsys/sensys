/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/diag/base/base.h"
#include "orcm/util/utils.h"
#include "orte/mca/errmgr/errmgr.h"

opal_list_t* orcm_diag_base_prepare_db_input(struct timeval start_time,
                                             struct timeval end_time,
                                             char *nodename, char *diag_type,
                                             char *diag_subtype, char *diag_result)
{
    opal_list_t *db_input = NULL;
    opal_value_t *kv = NULL;

    db_input = OBJ_NEW(opal_list_t);
    if (NULL != db_input) {
        /* load the start time */
        kv = orcm_util_load_opal_value("start_time", &start_time, OPAL_TIMEVAL);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the end time */
        kv = orcm_util_load_opal_value("end_time", &end_time, OPAL_TIMEVAL);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the hostname */
        kv = orcm_util_load_opal_value("hostname", nodename, OPAL_STRING);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the diag type */
        kv = orcm_util_load_opal_value("diag_type", diag_type, OPAL_STRING);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the diag subtype */
        kv = orcm_util_load_opal_value("diag_subtype", diag_subtype, OPAL_STRING);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));

        /* load the diag result */
        kv = orcm_util_load_opal_value("test_result", diag_result, OPAL_STRING);
        if (NULL == kv) {
            goto memtestcleanup;
        }
        opal_list_append(db_input, &(kv->super));
    }

    return db_input;

memtestcleanup:
    OPAL_LIST_RELEASE(db_input);
    return NULL;
}

void orcm_diag_base_db_cleanup(int db_handle, int status, opal_list_t *list,
                               opal_list_t *ret, void *cbdata)
{
    OPAL_LIST_RELEASE(list);

    if (ORTE_SUCCESS != status) {
        ORTE_ERROR_LOG(status);
    }
}
