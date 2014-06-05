/* -*- C -*-
 *
 * $HEADER$
 *
 */

#include <stdio.h>
#include "orcm/constants.h"
#include "orcm/types.h"

#include "opal/dss/dss.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"
#include "orte/util/proc_info.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_wait.h"

#include "orcm/mca/pvsn/base/base.h"
#include "orcm/runtime/runtime.h"
#include "orcm/mca/scd/scd.h"

static opal_cmd_line_init_t cmd_line_init[] = {
    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

static void callback(int status,
                     opal_list_t *images,
                     void *cbdata)
{
    bool *active = (bool*)cbdata;
    orcm_pvsn_resource_t *res;
    opal_value_t *kv;

    /* print the result */
    OPAL_LIST_FOREACH(res, images, orcm_pvsn_resource_t) {
        fprintf(stderr, "TYPE: %s\n", res->type);
        OPAL_LIST_FOREACH(kv, &res->attributes, opal_value_t) {
            opal_dss.dump(0, kv, OPAL_VALUE);
        }
    }

    *active = false;
}

int main(int argc, char* argv[])
{
    int rc, n;
    opal_cmd_line_t cmd_line;
    bool active;

    opal_cmd_line_create(&cmd_line, cmd_line_init);
    mca_base_cmd_line_setup(&cmd_line);
    if (OPAL_SUCCESS != (rc = opal_cmd_line_parse(&cmd_line, true,
                                                  argc, argv)) ) {
        if (OPAL_ERR_SILENT != rc) {
            fprintf(stderr, "%s: command line error (%s)\n", argv[0],
                    opal_strerror(rc));
        }
        return rc;
    }

    /*
     * Since this process can now handle MCA/GMCA parameters, make sure to
     * process them.
     */
    mca_base_cmd_line_process_args(&cmd_line, &environ, &environ);
    
    if (ORTE_SUCCESS != orcm_init(ORCM_TOOL)) {
        fprintf(stderr, "Failed orcm_init\n");
        exit(1);
    }
    
    /* open/select the provisioning framework */
    if (ORTE_SUCCESS != (rc = mca_base_framework_open(&orcm_pvsn_base_framework, 0))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    if (ORTE_SUCCESS != (rc = orcm_pvsn_base_select())) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    /* request available images */
    active = true;
    orcm_pvsn.get_available_images(NULL, callback, &active);
    ORTE_WAIT_FOR_COMPLETION(active);

    if (ORTE_SUCCESS != orcm_finalize()) {
        fprintf(stderr, "Failed orcm_finalize\n");
        exit(1);
    }
    return 0;
}
