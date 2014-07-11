/*
 * Copyright (c) 2014      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm_config.h"
#include "orcm/types.h"

#include <sys/types.h>

#include "opal_stdint.h"
#include "opal/util/argv.h"
#include "opal/dss/dss.h"
#include "opal/dss/dss_internal.h"

#include "orte/mca/errmgr/errmgr.h"

#include "orcm/mca/scd/scd_types.h"
#include "orcm/mca/scd/base/base.h"

int orcm_pack_alloc(opal_buffer_t *buffer, const void *src,
                    int32_t num_vals, opal_data_type_t type)
{
    int ret = OPAL_SUCCESS;
    orcm_alloc_t **allocs, *alloc;
    int32_t i;
    int32_t j;
    orcm_resource_t *r;
    int8_t k;

    /* array of pointers to orcm_alloc_t objects - need
     * to pack the objects a set of fields at a time
     */
    allocs = (orcm_alloc_t**)src;

    for (i=0; i < num_vals; i++) {
        alloc = allocs[i];
        //opal_dss.dump(0, alloc, ORCM_ALLOC);
        /* pack the id */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->id,
                                        1,
                                        OPAL_INT64))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the priority */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->priority,
                                        1,
                                        OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the account */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->account,
                                        1,
                                        OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the project name */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->name,
                                        1,
                                        OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the gid */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->gid,
                                        1,
                                        OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the max_nodes */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->max_nodes,
                                        1,
                                        OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the max_pes */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->max_pes,
                                        1,
                                        OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the min_nodes */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->min_nodes,
                                        1,
                                        OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the min_pes */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->min_pes,
                                        1,
                                        OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the beginning time */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->begin,
                                        1,
                                        OPAL_TIME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the walltime */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->walltime,
                                        1,
                                        OPAL_TIME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the exclusive flag */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->exclusive,
                                        1,
                                        OPAL_BOOL))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the caller_uid */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->caller_uid,
                                        1,
                                        OPAL_UINT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the caller_gid */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->caller_gid,
                                        1,
                                        OPAL_UINT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the interactive flag */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->interactive,
                                        1,
                                        OPAL_BOOL))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the originator */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->originator,
                                        1,
                                        ORTE_NAME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the hnp daemon */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->hnp,
                                        1,
                                        ORTE_NAME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the hnp name */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->hnpname,
                                        1,
                                        OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the hnp uri */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->hnpuri,
                                        1,
                                        OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the parent name */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->parent_name,
                                        1,
                                        OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the parent uri */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->parent_uri,
                                        1,
                                        OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the nodefile */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->nodefile,
                                        1,
                                        OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the nodes */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->nodes,
                                        1,
                                        OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the queues */
        if (OPAL_SUCCESS !=
            (ret = opal_dss_pack_buffer(buffer,
                                        (void*)&alloc->queues,
                                        1,
                                        OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* pack the resource constraints */
        j = (int32_t)opal_list_get_size(&alloc->constraints);
            if (OPAL_SUCCESS !=
                (ret = opal_dss_pack_buffer(buffer,
                                            (void*)&j,
                                            1,
                                            OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        if (0 < j) {
            OPAL_LIST_FOREACH(r, &alloc->constraints, orcm_resource_t) {
                k = r->type;
                if (OPAL_SUCCESS !=
                    (ret = opal_dss_pack_buffer(buffer,
                                                (void*)&k,
                                                1,
                                                OPAL_INT8))) {
                    ORTE_ERROR_LOG(ret);
                    return ret;
                }
                    if (OPAL_SUCCESS !=
                        (ret = opal_dss_pack_buffer(buffer,
                                                    (void*)&r->constraint,
                                                    1,
                                                    OPAL_STRING))) {
                    ORTE_ERROR_LOG(ret);
                    return ret;
                }
            }
        }
    }

    return ret;
}

int orcm_unpack_alloc(opal_buffer_t *buffer, void *dest,
                      int32_t *num_vals, opal_data_type_t type)
{
    int ret;
    int32_t i, n;
    orcm_alloc_t **allocs, *a;
    int32_t j, m;
    orcm_resource_t *r;
    int8_t k;

    /* unpack into array of orcm_alloc_t objects */
    allocs = (orcm_alloc_t**)dest;
    for (i=0; i < *num_vals; i++) {

        /* create the orcm_alloc_t object */
        allocs[i] = OBJ_NEW(orcm_alloc_t);
        if (NULL == allocs[i]) {
            ORTE_ERROR_LOG(ORTE_ERR_OUT_OF_RESOURCE);
            return ORTE_ERR_OUT_OF_RESOURCE;
        }
        a = allocs[i];

        /* unpack the id */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->id,
                                          &n,
                                          OPAL_INT64))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the priority */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->priority,
                                          &n,
                                          OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the account */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->account,
                                          &n,
                                          OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the project name */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->name,
                                          &n,
                                          OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the gid */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->gid,
                                          &n,
                                          OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the max_nodes */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->max_nodes,
                                          &n,
                                          OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the max_pes */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->max_pes,
                                          &n,
                                          OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the min_nodes */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->min_nodes,
                                          &n,
                                          OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the min_pes */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->min_pes,
                                          &n,
                                          OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the beginning time */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->begin,
                                          &n,
                                          OPAL_TIME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the walltime */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->walltime,
                                          &n,
                                          OPAL_TIME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the exclusive flag */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->exclusive,
                                          &n,
                                          OPAL_BOOL))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the caller_uid */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->caller_uid,
                                          &n,
                                          OPAL_UINT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the caller_gid */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->caller_gid,
                                          &n,
                                          OPAL_UINT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the interactive flag */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->interactive,
                                          &n,
                                          OPAL_BOOL))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the orginator */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->originator,
                                          &n,
                                          ORTE_NAME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the hnp daemon */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->hnp,
                                          &n,
                                          ORTE_NAME))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the hnp name */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->hnpname,
                                          &n,
                                          OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the hnp uri */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->hnpuri,
                                          &n,
                                          OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the parent name */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->parent_name,
                                          &n,
                                          OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the parent uri */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->parent_uri,
                                          &n,
                                          OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the nodefile */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->nodefile,
                                          &n,
                                          OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the nodes */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->nodes,
                                          &n,
                                          OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the queues */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &a->queues,
                                          &n,
                                          OPAL_STRING))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        /* unpack the constraints */
        n=1;
        if (OPAL_SUCCESS !=
            (ret = opal_dss_unpack_buffer(buffer,
                                          &j,
                                          &n,
                                          OPAL_INT32))) {
            ORTE_ERROR_LOG(ret);
            return ret;
        }
        for (m=0; m < j; m++) {
            r = OBJ_NEW(orcm_resource_t);
            n=1;
            if (OPAL_SUCCESS !=
                (ret = opal_dss_unpack_buffer(buffer,
                                              &k,
                                              &n,
                                              OPAL_INT8))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
            r->type = (orcm_resource_type_t)k;
            n=1;
            if (OPAL_SUCCESS !=
                (ret = opal_dss_unpack_buffer(buffer,
                                              &r->constraint,
                                              &n,
                                              OPAL_STRING))) {
                ORTE_ERROR_LOG(ret);
                return ret;
            }
            opal_list_append(&a->constraints, &r->super);
        }
    }

    return ORTE_SUCCESS;
}

int orcm_compare_alloc(orcm_alloc_t *value1, orcm_alloc_t *value2, opal_data_type_t type)
{
    if (value1->priority > value2->priority) return OPAL_VALUE1_GREATER;
    if (value2->priority > value1->priority) return OPAL_VALUE2_GREATER;
    
    return OPAL_EQUAL;
}


int orcm_copy_alloc(orcm_alloc_t **dest, orcm_alloc_t *src, opal_data_type_t type)
{
    (*dest) = src;
    OBJ_RETAIN(src);
    
    return ORTE_SUCCESS;
}

int orcm_print_alloc(char **output, char *prefix, orcm_alloc_t *src, opal_data_type_t type)
{
    char *tmp, *pfx;

    /* set default result */
    *output = NULL;

    /* protect against NULL prefix */
    if (NULL == prefix) {
        asprintf(&pfx, " ");
    } else {
        asprintf(&pfx, "%s", prefix);
    }

    asprintf(&tmp, "\n%sData for allocation object:\n\tPri: %d\tAccount: %s\tProject: %s",
             pfx, src->priority, (NULL == src->account) ? "N/A" : src->account,
             (NULL == src->name) ? "N/A" : src->name);

    asprintf(&tmp, "%s\n%s\tGID: %d\t#nodes: %d:%d\t#pes: %d:%d\tMode: %s",
             tmp, pfx, src->gid,
             src->max_nodes, src->min_nodes, src->max_pes, src->min_pes,
             src->exclusive ? "EXCLUSIVE" : "SHARED");

    asprintf(&tmp, "%s\n%s\tNodefile: %s\tNodes: %s\tQueues: %s",
             tmp, pfx,
             (NULL == src->nodefile) ? "N/A" : src->nodefile,
             (NULL == src->nodes) ? "N/A" : src->nodes,
             (NULL == src->queues) ? "N/A" : src->queues);

    asprintf(&tmp, "%s\n%s\tUSER: %u\tGROUP: %u\n",
             tmp, pfx, src->caller_uid, src->caller_gid);
    *output = tmp;
    
    free(pfx);

    return ORTE_SUCCESS;
}
