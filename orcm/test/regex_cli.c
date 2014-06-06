/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"

#include <stdio.h>
#include <unistd.h>

#include "orte/util/proc_info.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orcm/util/regex.h"
#include "orcm/runtime/runtime.h"

int main(int argc, char **argv)
{
    int rc;
    char *regex, *save;
    char *nodes=NULL;
    int i;
    struct timeval tv_start, tv_end;
    float elapsed=0;
    
    if (argc < 1 || NULL == argv[1]) {
        fprintf(stderr, "usage: regex <comma-separated list of nodes>|<regex>\n");
        return 1;
    }
    
    gettimeofday(&tv_start, 0);
    if (NULL != strchr(argv[1], '[')) {
        /* given a regex to analyze */
        if (ORCM_SUCCESS != (rc = orcm_regex_extract_names_list(argv[1], &nodes))) {
            ORTE_ERROR_LOG(rc);
        }
        fprintf(stderr, "NODELIST: %s\n", nodes);
        free(nodes);
    } else {
        save = strdup(argv[1]);
        if (ORCM_SUCCESS != (rc = orcm_regex_create(save, &regex))) {
            ORTE_ERROR_LOG(rc);
        } else {
            fprintf(stderr, "REGEX: %s\n", regex);
            free(regex);
        }
        free(save);
    }
    
    gettimeofday(&tv_end, 0);
    elapsed = (tv_end.tv_sec-tv_start.tv_sec)*1000000 + tv_end.tv_usec-tv_start.tv_usec;
    elapsed = elapsed/1000000;
    fprintf(stderr, "elapsed time: %g sec\n", elapsed);

    return 0;
}
