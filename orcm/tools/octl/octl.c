/*
 * Copyright (c) 2014-2016 Intel Corporation.  All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/tools/octl/common.h"

int
main(int argc, char *argv[])
{
    int erri = ORCM_SUCCESS;
    while (ORCM_SUCCESS == erri) {
        /* initialize, parse command line, and setup frameworks */
        erri = orcm_octl_work(argc, argv);
        if (ORCM_SUCCESS != erri) {
            break;
        }

        if (ORCM_SUCCESS != (erri = orcm_finalize())) {
            orcm_octl_error("orcm-finalize");
            break;
        }

        break;
    }

    return erri;
}
