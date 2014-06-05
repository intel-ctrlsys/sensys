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
#include <ctype.h>

#include "opal/util/argv.h"
#include "opal/util/opal_getcwd.h"
#include "opal/util/output.h"
#include "opal/util/path.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/show_help.h"

#include "orcm/mca/pvsn/base/base.h"
#include "pvsn_ww.h"

#define ORCM_MAX_LINE_LENGTH  1024

static int init(void);
static void finalize(void);
static int avail(char *resources, opal_list_t *available);
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
static char *parse_next(char *line, char **remainder);

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

static int avail(char *resources, opal_list_t *available)
{
    char *query, *line, *ptr;
    FILE *fp;
    orcm_pvsn_resource_t *res;
    opal_value_t *attr;
    char **types = NULL;
    int i, rc=ORTE_SUCCESS;
    int j;

    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                         "%s pvsn:wwulf:avail",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    /* A NULL for resources indicates the caller wants a list
     * of all known resource types. WW only has two of interest:
     * vnfs images and files */
    if (NULL == resources) {
        opal_argv_append_nosize(&types, "images");
        opal_argv_append_nosize(&types, "files");
    } else {
        /* the resource request can contain a comma-separated list
         * of desired resource types, so split it here */
        types = opal_argv_split(resources, ',');
    }

    for (i=0; NULL != types[i]; i++) {
        OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                             "%s pvsn:wwulf:avail looking for resource type %s",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), types[i]));
        if (0 == strcasecmp(types[i], "images")) {
            OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                                 "%s pvsn:wwulf:avail getting vnfs list",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
            /* get the vnfs image list */
            (void)asprintf(&query, "%s vnfs list", cmd);
            if (NULL == (fp = popen(query, "r"))) {
                OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                                     "%s pvsn:wwulf:avail query for resource type %s failed",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), types[i]));
                free(query);
                rc = ORTE_ERROR;
                break;
            }
            free(query);
            while (NULL != (line = orcm_getline(fp))) {
                OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                                     "%s pvsn:wwulf:avail got input %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), line));
                /* we need to discard the header, so check for it */
                if (0 == strncmp(line, "VNFS NAME", strlen("VNFS NAME"))) {
                    /* this is the header - ignore it */
                    OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                                         "%s pvsn:wwulf:avail ignoring header",
                                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
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
                res = OBJ_NEW(orcm_pvsn_resource_t);
                res->type = strdup("image");
                opal_list_append(available, &res->super);
                attr = OBJ_NEW(opal_value_t);
                attr->key = strdup("name");
                attr->type = OPAL_STRING;
                attr->data.string = strdup(query);
                opal_list_append(&res->attributes, &attr->super);
                if (NULL == (query = parse_next(ptr, &ptr))) {
                    /* should have been the size, but no other info
                     * was given - for now, don't worry about it */
                    free(line);
                    continue;
                }
                attr = OBJ_NEW(opal_value_t);
                attr->key = strdup("size");
                attr->type = OPAL_FLOAT;
                attr->data.fval = strtof(query, NULL);
                opal_list_append(&res->attributes, &attr->super);
                if (NULL == (query = parse_next(ptr, &ptr))) {
                    /* should have been the location, but no other info
                     * was given - for now, don't worry about it */
                    free(line);
                    continue;
                }
                attr = OBJ_NEW(opal_value_t);
                attr->key = strdup("location");
                attr->type = OPAL_STRING;
                attr->data.string = strdup(query);
                opal_list_append(&res->attributes, &attr->super);
                free(line);
            }
            pclose(fp);
        } else if (0 == strcasecmp(types[i], "files")) {
            OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                                 "%s pvsn:wwulf:avail getting files list",
                                 ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));
            /* get the files list */
            (void)asprintf(&query, "%s file list", cmd);
            if (NULL == (fp = popen(query, "r"))) {
                OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                                     "%s pvsn:wwulf:avail query for resource type %s failed",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), types[i]));
                free(query);
                rc = ORTE_ERROR;
                break;
            }
            free(query);
            while (NULL != (line = orcm_getline(fp))) {
                OPAL_OUTPUT_VERBOSE((5, orcm_pvsn_base_framework.framework_output,
                                     "%s pvsn:wwulf:avail got input %s",
                                     ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), line));
                /* we want the following sections of the output line:
                 * 0  => ww file name
                 * 2  => permissions
                 * 4  => owner
                 * 5  => group
                 * 6  => size
                 * 7  => path
                 */
                ptr = line;
                j=0;
                while (NULL != (query = parse_next(ptr, &ptr))) {
                    switch(j) {
                    case 0:
                        attr = OBJ_NEW(opal_value_t);
                        attr->key = strdup("name");
                        attr->type = OPAL_STRING;
                        attr->data.string = strdup(query);
                        opal_list_append(&res->attributes, &attr->super);
                        break;
                    case 2:
                        attr = OBJ_NEW(opal_value_t);
                        attr->key = strdup("permissions");
                        attr->type = OPAL_STRING;
                        attr->data.string = strdup(query);
                        opal_list_append(&res->attributes, &attr->super);
                        break;
                    case 4:
                        attr = OBJ_NEW(opal_value_t);
                        attr->key = strdup("owner");
                        attr->type = OPAL_STRING;
                        attr->data.string = strdup(query);
                        opal_list_append(&res->attributes, &attr->super);
                        break;
                    case 5:
                        attr = OBJ_NEW(opal_value_t);
                        attr->key = strdup("group");
                        attr->type = OPAL_STRING;
                        attr->data.string = strdup(query);
                        opal_list_append(&res->attributes, &attr->super);
                        break;
                    case 6:
                        attr = OBJ_NEW(opal_value_t);
                        attr->key = strdup("size");
                        attr->type = OPAL_SIZE;
                        attr->data.size = (size_t)strtoul(query, NULL, 10);
                        opal_list_append(&res->attributes, &attr->super);
                        break;
                    case 7:
                        attr = OBJ_NEW(opal_value_t);
                        attr->key = strdup("path");
                        attr->type = OPAL_STRING;
                        attr->data.string = strdup(query);
                        opal_list_append(&res->attributes, &attr->super);
                        break;
                    default:
                        /* just ignore it */
                        break;
                    }
                    j++;
                }
                free(line);
            }
        } else {
            orte_show_help("help-pvsn-ww.txt", "unknown-type", true, types[i]);
            rc = ORTE_ERR_BAD_PARAM;
            break;
        }
    }

    if (NULL != types) {
        opal_argv_free(types);
    }

    return rc;
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

static char *parse_next(char *line, char **remainder)
{
    char *start, *end;

    /* check for end */
    if (NULL == line) {
        *remainder = NULL;
        return NULL;
    }

    /* find the starting point - first non-whitespace */
    start = line;
    while ('\0' != *start && isspace(*start)) {
        start++;
    }

    /* if we hit the end, then there wasn't anything on the line */
    if (NULL == start) {
        *remainder = NULL;
        return NULL;
    }

    /* find the end of the entry */
    end = start;
    while ('\0' != *end && !isspace(*end)) {
        end++;
    }

    /* if we are at the end, then there is no remainder */
    if ('\0' == *end) {
        *remainder = NULL;
    } else {
        /* mark the end */
        *end = '\0';
        end++;
        *remainder = end;
    }

    return start;
}
