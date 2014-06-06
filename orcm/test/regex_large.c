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
    char *regex;
    char *nodes=NULL;
    long numnodes, i;
    struct timeval tv_start, tv_end;
    float elapsed=0;
    
    if (argc < 1 || NULL == argv[1]) {
        fprintf(stderr, "usage: regex_large <number of nodes>\n");
        return 1;
    }
    
    numnodes = strtol(argv[1], '\0', 10);
    for (i = 1; i <= numnodes; i++) {
        if (1 == i) {
            asprintf(&nodes, "node%05ld", i);
        } else {
            asprintf(&nodes, "%s,node%05ld",nodes, i);
        }
    }
    
    gettimeofday(&tv_start, 0);
    
    if (ORCM_SUCCESS != (rc = orcm_regex_create(nodes, &regex))) {
        ORTE_ERROR_LOG(rc);
    } else {
        fprintf(stderr, "REGEX: %s\n", regex);
        free(regex);
    }
    
    gettimeofday(&tv_end, 0);
    elapsed = (tv_end.tv_sec-tv_start.tv_sec)*1000000 + tv_end.tv_usec-tv_start.tv_usec;
    elapsed = elapsed/1000000;
    fprintf(stderr, "elapsed time: %g sec\n", elapsed);
    
    return 0;
}