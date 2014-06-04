/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <stdio.h>

#include "opal/util/opal_getcwd.h"
#include "opal/util/output.h"
#include "opal/util/path.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/pvsn/base/base.h"
#include "pvsn_ww.h"

#define ORCM_MAX_LINE_LENGTH  1024

static int init(void);
static void finalize(void);
static int avail(opal_list_t *images);
static int status(char *nodes, opal_list_t *images);
static int provision(char *nodes,
                     char *image,
                     opal_list_t *attributes);

orcm_pvsn_base_module_t orcm_pvsn_wwulf_module = {
    init,
    finalize,
    avail,
    status,
    provision
};

static char *cmd = NULL;
static char *orcm_getline(FILE *fp);
static char *parse_next(char *line, char **ptr);

static int init(void)
{
    char cwd[OPAL_PATH_MAX];
    int rc;

    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:init",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    
    if (OPAL_SUCCESS != (rc = opal_getcwd(cwd, OPAL_PATH_MAX))) {
        return rc;
    }

    /* check to see if we can execute wwsh */
    cmd = opal_path_findv("wwsh", X_OK, environ, cwd);
    if (NULL == cmd) {
        return ORTE_ERR_EXE_NOT_FOUND;
    }

    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:init path to wwsh %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), cmd));

    return ORCM_SUCCESS;
}

static void finalize(void)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:finalize",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    if (NULL != cmd) {
        free(cmd);
    }
}

static int avail(opal_list_t *images)
{
    char *query, *line, *ptr;
    FILE *fp;
    orcm_pvsn_image_t *img;

    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:avail",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* get the vnfs image list */
    (void)asprintf(&query, "%s vnfs list", cmd);
    if (NULL == (fp = popen(query, "r"))) {
        free(query);
        return ORTE_ERROR;
    }
    free(query);
    while (NULL != (line = orcm_getline(fp))) {
        /* we need to discard the header, so check for it */
        if (0 == strncmp(line, "VNFS NAME", strlen("VNFS NAME"))) {
            /* this is the header - ignore it */
            free(line);
            continue;
        }
        /* break into three sections - the first contains the
         * image name, the second the size, and the third the
         * chroot location */
        ptr = line;
        if (NULL == (query = parse_next(line, &ptr))) {
            /* not even an image name was given */
            free(line);
            continue;
        }
        img = OBJ_NEW(orcm_pvsn_image_t);
        img->image = query;
        opal_list_append(images, &img->super);
        if (NULL == ptr) {
            /* nothing else on line */
            free(line);
            continue;
        }
        if (NULL == (query = parse_next(line, &ptr))) {
            /* should have been the size, but no other info
             * was given - for now, don't worry about it */
            free(line);
            continue;
        }
        free(line);
    }
    pclose(fp);
    return ORCM_SUCCESS;
}

static int status(char *nodes, opal_list_t *images)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:status",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    return ORCM_ERR_NOT_IMPLEMENTED;
}

static int provision(char *nodes,
                     char *image,
                     opal_list_t *attributes)
{
    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:provision",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
    return ORCM_ERR_NOT_IMPLEMENTED;
}


static char *orcm_getline(FILE *fp)
{
    char *ret, *buff, *ptr;
    char input[ORCM_MAX_LINE_LENGTH];
    size_t i;

    ret = fgets(input, ORCM_MAX_LINE_LENGTH, fp);
    if (NULL != ret) {
	   input[strlen(input)-1] = '\0';  /* remove newline */
           /* strip leading spaces */
           ptr = input;
           for (i=0; i < strlen(input)-1; i++) {
               if (' ' != input[i]) {
                   ptr = &input[i];
                   break;
               }
           }
	   buff = strdup(ptr);
	   return buff;
    }
    
    return NULL;
}

static char *parse_next(char *line, char **ptr)
{
}
