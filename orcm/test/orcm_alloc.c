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

#include "orcm/runtime/runtime.h"
#include "orcm/mca/scd/scd.h"

static opal_cmd_line_init_t cmd_line_init[] = {
    /* End of list */
    { NULL, '\0', NULL, NULL, 0,
      NULL, OPAL_CMD_LINE_TYPE_NULL, NULL }
};

int main(int argc, char* argv[])
{
    orcm_alloc_t alloc, *aptr;
    orte_rml_recv_cb_t xfer;
    opal_buffer_t *buf;
    int rc, n;
    opal_cmd_line_t cmd_line;
    orcm_sched_cmd_flag_t command=ORCM_SESSION_REQ_COMMAND;
    orcm_alloc_id_t id;

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
    
    /* create an allocation request */
    OBJ_CONSTRUCT(&alloc, orcm_alloc_t);
    alloc.account = "Ralph";
    alloc.name = "test";

    /* setup to receive the result */
    OBJ_CONSTRUCT(&xfer, orte_rml_recv_cb_t);
    xfer.active = true;
    orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                            ORCM_RML_TAG_ALLOC,
                            ORTE_RML_NON_PERSISTENT,
                            orte_rml_recv_callback, &xfer);

    /* send it to the scheduler */
    buf = OBJ_NEW(opal_buffer_t);
    /* pack the alloc command flag */
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &command,1, ORCM_SCHED_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }

    aptr = &alloc;
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buf, &aptr, 1, ORCM_ALLOC))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(ORTE_PROC_MY_SCHEDULER, buf,
                                                      ORCM_RML_TAG_ALLOC,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(buf);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }

    /* get our allocated jobid */
    n=1;
    ORTE_WAIT_FOR_COMPLETION(xfer.active);
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(&xfer.data, &id, &n, ORCM_ALLOC_ID_T))) {
        ORTE_ERROR_LOG(rc);
        OBJ_DESTRUCT(&xfer);
        return rc;
    }
    opal_output(0, "RECEIVED ALLOC ID %d", id);

    /* release the allocation */

    if (ORTE_SUCCESS != orcm_finalize()) {
        fprintf(stderr, "Failed orcm_finalize\n");
        exit(1);
    }
    return 0;
}
