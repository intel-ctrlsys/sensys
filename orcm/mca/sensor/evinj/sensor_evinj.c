/*
 * Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2011-2012 Los Alamos National Security, LLC.
 *                         All rights reserved.
 * Copyright (c) 2014-2015 Intel, Inc.  All rights reserved.
 *
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
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#include <ctype.h>
#include <time.h>

#include "opal_stdint.h"
#include "opal/util/alfg.h"
#include "opal/util/output.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/mca/evgen/base/base.h"
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"
#include "sensor_evinj.h"

/* declare the API functions */
static int init(void);
static void finalize(void);
static void sample(orcm_sensor_sampler_t *sampler);

/* instantiate the module */
orcm_sensor_base_module_t orcm_sensor_evinj_module = {
    init,
    finalize,
    NULL,
    NULL,
    sample,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

FILE *fp = NULL;

static char *orcm_getline(void)
{
    char *ret, *buff;
    char input[1024];
    int k;

    ret = fgets(input, 1024, fp);
    if (NULL != ret) {
        /* trim the end of the line */
        for (k=strlen(input)-1; 0 < k && isspace(input[k]); k--) {
            input[k] = '\0';
        }
        buff = strdup(input);
        return buff;
    }

    return NULL;
}

static int init(void)
{
    /* if we were given one, open the vector file */
    if (NULL != mca_sensor_evinj_component.vector_file) {
        fp = fopen(mca_sensor_evinj_component.vector_file, "r");
    }

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    /* close the vector file */
    if (NULL != fp) {
        fclose(fp);
    }
}

static void sample(orcm_sensor_sampler_t *sampler)
{
    float prob, division, check;
    char *vector, **elements, **parts, **pieces;
    orcm_ras_event_t *rev;
    int i, j;

    OPAL_OUTPUT_VERBOSE((1, orcm_sensor_base_framework.framework_output,
                         "%s sample:evinj considering injecting something",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

     /* roll the dice */
    prob = (double)opal_rand(&mca_sensor_evinj_component.rng_buff) / (double)UINT32_MAX;
    if (prob < mca_sensor_evinj_component.prob) {
        rev = OBJ_NEW(orcm_ras_event_t);
        /* if we were given a vector file, read
         * the next vector from the file */
        if (NULL != fp) {
            vector = orcm_getline();
            if (NULL == vector) {
                /* reopen the file to start over */
                fclose(fp);
                fp = fopen(mca_sensor_evinj_component.vector_file, "r");
                if (NULL == fp) {
                    /* nothing we can do */
                    return;
                }
                vector = orcm_getline();
                if (NULL == vector) {
                    /* give up */
                    return;
                }
            }
            elements = opal_argv_split(vector, ';');
            free(vector);
            i=0;
            /* first field must contain a comma-delimited set of descriptors
             * of the location reporting this event, each descriptor given
             * as a colon-separated key:value pair (only string values are
             * supported when read from a file) */
            parts = opal_argv_split(elements[i], ',');
            for (j=0; NULL != parts[j]; j++) {
                pieces = opal_argv_split(parts[j], ':');
                if (2 != opal_argv_count(pieces)) {
                    ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                    opal_argv_free(elements);
                    opal_argv_free(parts);
                    opal_argv_free(pieces);
                    OBJ_RELEASE(rev);
                    return;
                }
                ORCM_RAS_REPORTER(rev, pieces[0], pieces[1], OPAL_STRING);
                opal_argv_free(pieces);
            }
            opal_argv_free(parts);
            /* next field must be the event type */
            ++i;
            if (0 == strcmp("EXCEPTION", elements[i])) {
                rev->type = ORCM_RAS_EVENT_EXCEPTION;
            } else if (0 == strcmp("TRANSITION", elements[i])) {
                rev->type = ORCM_RAS_EVENT_STATE_TRANSITION;
            } else if (0 == strcmp("SENSOR", elements[i])) {
                rev->type = ORCM_RAS_EVENT_SENSOR;
            } else if (0 == strcmp("COUNTER", elements[i])) {
                rev->type = ORCM_RAS_EVENT_COUNTER;
            } else {
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                opal_argv_free(elements);
                OBJ_RELEASE(rev);
                return;
            }
            /* next field must be the severity */
            ++i;
            if (0 == strcmp("EMERGENCY", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_EMERG;
            } else if (0 == strcmp("FATAL", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_FATAL;
            } else if (0 == strcmp("ALERT", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_ALERT;
            } else if (0 == strcmp("CRITICAL", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_CRIT;
            } else if (0 == strcmp("ERROR", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_ERROR;
            } else if (0 == strcmp("WARNING", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_WARNING;
            } else if (0 == strcmp("NOTICE", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_NOTICE;
            } else if (0 == strcmp("INFO", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_INFO;
            } else if (0 == strcmp("TRACE", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_TRACE;
            } else if (0 == strcmp("DEBUG", elements[i])) {
                rev->severity = ORCM_RAS_SEVERITY_DEBUG;
            } else {
                ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                opal_argv_free(elements);
                OBJ_RELEASE(rev);
                return;
            }
            /* next field is optional - if provided, it will consist
             * of a comma-delimited set of descriptors for this
             * event, each given as a colon-separated key:value pair
             * (only string values are supported when read from a file) */
            ++i;
            if (NULL == elements[i]) {
                /* we are done */
                opal_argv_free(elements);
                goto execute;
            }
            parts = opal_argv_split(elements[i], ',');
            for (j=0; NULL != parts[j]; j++) {
                pieces = opal_argv_split(parts[j], ':');
                if (2 != opal_argv_count(pieces)) {
                    ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                    opal_argv_free(elements);
                    opal_argv_free(parts);
                    opal_argv_free(pieces);
                    OBJ_RELEASE(rev);
                    return;
                }
                ORCM_RAS_DESCRIPTION(rev, pieces[0], pieces[1], OPAL_STRING);
                opal_argv_free(pieces);
            }
            opal_argv_free(parts);
             /* the final field is also optional - if provided it
             * will consist of a comma-delimited set of data elements for this
             * event, each given as a colon-separated key:value pair
             * (only string values are supported when read from a file)*/
            ++i;
            if (NULL == elements[i]) {
                /* we are done */
                opal_argv_free(elements);
                goto execute;
            }
            parts = opal_argv_split(elements[i], ',');
            for (j=0; NULL != parts[j]; j++) {
                pieces = opal_argv_split(parts[j], ':');
                if (3 != opal_argv_count(pieces)) {
                    ORTE_ERROR_LOG(ORCM_ERR_BAD_PARAM);
                    opal_argv_free(elements);
                    opal_argv_free(parts);
                    opal_argv_free(pieces);
                    OBJ_RELEASE(rev);
                    return;
                }
                ORCM_RAS_DATA(rev, pieces[0], pieces[1], OPAL_STRING);
                opal_argv_free(pieces);
            }
            opal_argv_free(parts);
            opal_argv_free(elements);
        } else {
            /* just use some bogus location for test purposes */
            ORCM_RAS_REPORTER(rev, ORCM_LOC_CLUSTER, "GRAND-SLAM", OPAL_STRING);
            ORCM_RAS_REPORTER(rev, ORCM_LOC_ROW, "a", OPAL_STRING);
            i = 3;
            ORCM_RAS_REPORTER(rev, ORCM_LOC_RACK, &i, OPAL_INT);
            ORCM_RAS_REPORTER(rev, ORCM_LOC_NODE, "a305", OPAL_STRING);
            ORCM_RAS_REPORTER(rev, ORCM_COMPONENT_OVLYNET, ORCM_SUBCOMPONENT_PROC, OPAL_STRING);
            /* randomly generate the event type */
            prob = (double)opal_rand(&mca_sensor_evinj_component.rng_buff) / (double)UINT32_MAX;
            division = 1.0 / (float)(ORCM_RAS_EVENT_UNKNOWN_TYPE+1);
            rev->type = 0;
            for (check=division; check < prob; check += division) {
                ++rev->type;
            }
            /* randomly generate the severity */
            prob = (double)opal_rand(&mca_sensor_evinj_component.rng_buff) / (double)UINT32_MAX;
            division = 1.0 / (float)(ORCM_RAS_SEVERITY_UNKNOWN+1);
            rev->severity = 0;
            for (check=division; check < prob; check += division) {
                ++rev->severity;
            }
            /* provide some description */
            check = 198.75;
            ORCM_RAS_DESCRIPTION(rev, ORCM_DESC_TEMP_HI, &check, OPAL_FLOAT);
            i = 13789;
            ORCM_RAS_DESCRIPTION(rev, ORCM_DESC_SESSION_ID, &i, OPAL_INT);
            /* provide some data */
            check = 134.8;
            ORCM_RAS_DATA(rev, "outlet avg temp", &check, OPAL_FLOAT);
        }

      execute:
        opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                             "%s sample:evinj injecting RAS event",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

        /* inject it into the event generator thread */
        ORCM_RAS_EVENT(rev);
    }
}
