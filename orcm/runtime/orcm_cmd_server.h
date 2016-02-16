/*
 * Copyright (c) 2016      Intel Corp, Inc.  All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

/**
 * @file
 *
 * Data server for OpenRTE
 */
#ifndef ORTE_DATA_SERVER_H
#define ORTE_DATA_SERVER_H

#include "orcm_config.h"
#include "orcm/types.h"

#include "opal/dss/dss_types.h"
#include "orte/mca/rml/rml_types.h"

BEGIN_C_DECLS

/* provide hooks to startup and finalize the cmd server */
ORCM_DECLSPEC int orcm_cmd_server_init(void);
ORCM_DECLSPEC void orcm_cmd_server_finalize(void);

/* provide hook for the non-blocking receive */
ORCM_DECLSPEC void orcm_cmd_server_recv(int status, orte_process_name_t* sender,
                                    opal_buffer_t* buffer, orte_rml_tag_t tag,
                                    void* cbdata);


END_C_DECLS

#endif /* ORTE_DATA_SERVER_H */
