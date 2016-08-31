/*
 * Copyright (c) 2014-2016      Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif  /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif  /* HAVE_STRING_H */
#include <ctype.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif

#include "opal/dss/dss.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal/util/opal_environ.h"
#include "opal/util/if.h"
#include "opal/util/net.h"
#include "opal/util/show_help.h"
#include "opal/mca/if/if.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/util/show_help.h"
#include "orte/runtime/orte_globals.h"

#include "orcm/runtime/orcm_globals.h"
#include "orcm/util/utils.h"

#define ORCM_MAX_LINE_LENGTH  512

static int get_port_number(char* port_string){
    int port = atoi(port_string);
    if (0 > port || USHRT_MAX < port){
        int overflow = 0xFFFF&port;
        int new_port = overflow<1024 ? overflow+1024 : overflow;

        opal_output(0, "ERROR: The port number %d is not in an acceptable "
                    "range [0 - 65535]. The application will use port %d "
                    "instead.", port, new_port);
        return new_port;
    }
    return port;
}

void orcm_util_construct_uri(opal_buffer_t *buf, orcm_node_t *node)
{
    char *uri;
    char *proc_name;
    char *addr;
    struct hostent *h;

    if (0 == strcmp(node->name, orte_process_info.nodename) ||
        0 == strcmp(node->name, "localhost") ||
        opal_ifislocal(node->name)) {
        /* just use "localhost" */
        addr = "localhost";
    } else if (opal_net_isaddr(node->name)) {
        addr = node->name;
    } else {
        /* lookup the address of this node */
        if (NULL == (h = gethostbyname(node->name))) {
            opal_output_verbose(2, orcm_debug_output,
                                "%s cannot resolve node %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), node->name);
            return;
        }
        addr = inet_ntoa(*(struct in_addr*)h->h_addr_list[0]);
    }
    int port = get_port_number(node->config.port);
    orte_util_convert_process_name_to_string(&proc_name, &node->daemon);
    asprintf(&uri, "%s;tcp://%s:%d", proc_name, addr, port);
    opal_output_verbose(2, orcm_debug_output,
                        "%s orcm:util: node %s addr %s uri %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                        node->name, addr, uri);
    opal_dss.pack(buf, &uri, 1, OPAL_STRING);
    free(proc_name);
    free(uri);
}

int orcm_util_get_dependents(opal_list_t *targets,
                             orte_process_name_t *root)
{
    orcm_cluster_t *cluster;
    orcm_row_t *row;
    orcm_rack_t *rack;
    orcm_node_t *node;
    orte_namelist_t *nm;

    /* cycle thru the clusters until we find the controller
     * that matches the given name - dependents include
     * everything below it
     */
    OPAL_LIST_FOREACH(cluster, orcm_clusters, orcm_cluster_t) {
        node = &cluster->controller;
        if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &node->daemon, root)) {
            OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
                node = &row->controller;
                nm = OBJ_NEW(orte_namelist_t);
                nm->name.jobid = node->daemon.jobid;
                nm->name.vpid = node->daemon.vpid;
                opal_list_append(targets, &nm->super);
                OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                    node = &rack->controller;
                    nm = OBJ_NEW(orte_namelist_t);
                    nm->name.jobid = node->daemon.jobid;
                    nm->name.vpid = node->daemon.vpid;
                    opal_list_append(targets, &nm->super);
                    OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                        nm = OBJ_NEW(orte_namelist_t);
                        nm->name.jobid = node->daemon.jobid;
                        nm->name.vpid = node->daemon.vpid;
                        opal_list_append(targets, &nm->super);
                    }
                }
            }
            return ORCM_SUCCESS;
        }

        OPAL_LIST_FOREACH(row, &cluster->rows, orcm_row_t) {
            node = &row->controller;
            if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &node->daemon, root)) {
                OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                    node = &rack->controller;
                    nm = OBJ_NEW(orte_namelist_t);
                    nm->name.jobid = node->daemon.jobid;
                    nm->name.vpid = node->daemon.vpid;
                    opal_list_append(targets, &nm->super);
                    OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                        nm = OBJ_NEW(orte_namelist_t);
                        nm->name.jobid = node->daemon.jobid;
                        nm->name.vpid = node->daemon.vpid;
                        opal_list_append(targets, &nm->super);
                    }
                }
                return ORCM_SUCCESS;
            }

            OPAL_LIST_FOREACH(rack, &row->racks, orcm_rack_t) {
                node = &rack->controller;
                if (OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &node->daemon, root)) {
                    OPAL_LIST_FOREACH(node, &rack->nodes, orcm_node_t) {
                        nm = OBJ_NEW(orte_namelist_t);
                        nm->name.jobid = node->daemon.jobid;
                        nm->name.vpid = node->daemon.vpid;
                        opal_list_append(targets, &nm->super);
                    }
                    return ORCM_SUCCESS;
                }
            }
        }
    }
    return ORCM_ERR_NOT_FOUND;
}

void orcm_util_print_xml(orcm_cfgi_xml_parser_t *x, char *pfx)
{
    int i;
    orcm_cfgi_xml_parser_t *y;
    char *p2;

    if (NULL == pfx) {
        opal_output(0, "tag: %s", x->name);
    } else {
        opal_output(0, "%stag: %s", pfx, x->name);
    }
    if (NULL != x->value) {
        for (i=0; NULL != x->value[i]; i++) {
            if (NULL == pfx) {
                opal_output(0, "    value: %s", x->value[i]);
            } else {
                opal_output(0, "%s    value: %s", pfx, x->value[i]);
            }
        }
    }
    if (NULL == pfx) {
        p2 = strdup("    ");
    } else {
        asprintf(&p2, "%s    ", pfx);
    }
    OPAL_LIST_FOREACH(y, &x->subvals, orcm_cfgi_xml_parser_t) {
        orcm_util_print_xml(y, p2);
    }
    free(p2);
}

opal_value_t* orcm_util_load_opal_value(char *key, void *data, opal_data_type_t type)
{
    int rc = -1;
    opal_value_t *kv = OBJ_NEW(opal_value_t);

    if (NULL != kv) {
        if (NULL != key) {
            kv->key = strdup(key);
        }
        rc = opal_value_load(kv, data, type);
        if (ORCM_SUCCESS != rc) {
            OBJ_RELEASE(kv);
            kv = NULL;
        }
    }

    return kv;
}


static int orcm_util_copy_opal_value_data(opal_value_t *dest, opal_value_t *src)
{
    if (NULL == dest) {
        if (OPAL_STRING != src->type && OPAL_BYTE_OBJECT != src->type) {
            return ORCM_SUCCESS;
        }
        return ORCM_ERR_COPY_FAILURE;
    }

    switch (src->type) {
    case OPAL_BOOL:
        dest->data.flag = src->data.flag;
        break;
    case OPAL_BYTE:
        dest->data.byte = src->data.byte;
        break;
    case OPAL_STRING:
        SAFEFREE(dest->data.string);
        if (NULL != src->data.string) {
            dest->data.string = strdup(src->data.string);
            if (NULL == dest->data.string) {
                return ORCM_ERR_OUT_OF_RESOURCE;
            }
        }
        break;
    case OPAL_SIZE:
        dest->data.size = src->data.size;
        break;
    case OPAL_PID:
        dest->data.pid = src->data.pid;
        break;

    case OPAL_INT:
        dest->data.integer = src->data.integer;
        break;
    case OPAL_INT8:
        dest->data.int8 = src->data.int8;
        break;
    case OPAL_INT16:
        dest->data.int16 = src->data.int16;
        break;
    case OPAL_INT32:
        dest->data.int32 = src->data.int32;
        break;
    case OPAL_INT64:
        dest->data.int64 = src->data.int64;
        break;

    case OPAL_UINT:
        dest->data.uint = src->data.uint;
        break;
    case OPAL_UINT8:
        dest->data.uint8 = src->data.uint8;
        break;
    case OPAL_UINT16:
        dest->data.uint16 = src->data.uint16;
        break;
    case OPAL_UINT32:
        dest->data.uint32 = src->data.uint32;
        break;
    case OPAL_UINT64:
        dest->data.uint64 = src->data.uint64;
        break;

    case OPAL_BYTE_OBJECT:
        if (NULL != dest->data.bo.bytes) {
            free(dest->data.bo.bytes);
        }
        if (NULL != src->data.bo.bytes && 0 < src->data.bo.size) {
            dest->data.bo.bytes = (uint8_t *) malloc(src->data.bo.size);
            memcpy(dest->data.bo.bytes, src->data.bo.bytes, src->data.bo.size);
            dest->data.bo.size = src->data.bo.size;
        } else {
            dest->data.bo.bytes = NULL;
            dest->data.bo.size = 0;
        }
        break;

    case OPAL_FLOAT:
        dest->data.fval = src->data.fval;
        break;

    case OPAL_DOUBLE:
        dest->data.dval = src->data.dval;
        break;

    case OPAL_TIMEVAL:
        dest->data.tv.tv_sec = src->data.tv.tv_sec;
        dest->data.tv.tv_usec = src->data.tv.tv_usec;
        break;

    case OPAL_PTR:
        dest->data.ptr = src->data.ptr;
        break;

    default:
        OPAL_ERROR_LOG(OPAL_ERR_NOT_SUPPORTED);
        return ORCM_ERR_NOT_SUPPORTED;
    }

    return ORCM_SUCCESS;

}

orcm_value_t* orcm_util_copy_orcm_value(orcm_value_t* src)
{
    int rc = -1;
    orcm_value_t *dest;

    if (NULL == src) {
        return NULL;
    }

    dest = OBJ_NEW(orcm_value_t);
    if (NULL != dest) {
        if (NULL != src->value.key) {
            dest->value.key = strdup(src->value.key);
            if (NULL == dest->value.key) {
                return NULL;
            }
        }
        dest->value.type = src->value.type;
        rc = orcm_util_copy_opal_value_data(&dest->value, &src->value);
        if (ORCM_SUCCESS != rc) {
            OBJ_RELEASE(dest);
            dest = NULL;
            return dest;
        }
        if ( NULL != src->units ) {
            dest->units = strdup(src->units);
            if (NULL == dest->units) {
                return NULL;
            }
        }
    }
    return dest;
}

orcm_value_t* orcm_util_load_orcm_value(char *key, void *data, opal_data_type_t type, char *units)
{
    int rc = -1;
    orcm_value_t *kv = OBJ_NEW(orcm_value_t);

    if (NULL != kv) {
        if (NULL != key) {
            kv->value.key = strdup(key);
            if (NULL == kv->value.key) {
                return NULL;
            }
        }
        rc = opal_value_load(&kv->value, data, type);
        if (ORCM_SUCCESS != rc) {
            OBJ_RELEASE(kv);
            kv = NULL;
            return kv;
        }
        if ( NULL != units ) {
            kv->units = strdup(units);
            if (NULL == kv->units) {
                return NULL;
            }
        }
    }
    return kv;
}

int orcm_util_append_orcm_value(opal_list_t *list, char *key, void *data,
                                opal_data_type_t type, char *units)
{
    orcm_value_t *list_item = NULL;

    if (NULL == key || NULL == data) {
        return ORCM_ERR_BAD_PARAM;
    }
    if (NULL == (list_item = orcm_util_load_orcm_value(key, data, type, units))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    opal_list_append(list, (opal_list_item_t *)list_item);

    return ORCM_SUCCESS;
}

int orcm_util_prepend_orcm_value(opal_list_t *list, char *key, void *data,
                                opal_data_type_t type, char *units)
{
    orcm_value_t *list_item = NULL;

    if (NULL == key || NULL == data) {
        return ORCM_ERR_BAD_PARAM;
    }
    if (NULL == (list_item = orcm_util_load_orcm_value(key, data, type, units))) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    opal_list_prepend(list, (opal_list_item_t *)list_item);

    return ORCM_SUCCESS;
}

orcm_analytics_value_t* orcm_util_load_orcm_analytics_value(opal_list_t *key,
                                                            opal_list_t *non_compute,
                                                            opal_list_t *compute)
{
    orcm_analytics_value_t *analytics_vals = OBJ_NEW(orcm_analytics_value_t);

    if (NULL != analytics_vals) {
        if (NULL != key) {
            OBJ_RETAIN(key);
            analytics_vals->key = key;
        }
        else {
            analytics_vals->key = OBJ_NEW(opal_list_t);
            if (NULL == analytics_vals->key) {
                ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
                return NULL;
            }

        }

        if (NULL != non_compute) {
            OBJ_RETAIN(non_compute);
            analytics_vals->non_compute_data = non_compute;
        }
        else {
            analytics_vals->non_compute_data = OBJ_NEW(opal_list_t);
            if (NULL == analytics_vals->non_compute_data) {
                ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
                return NULL;
            }

        }

        if (NULL != compute) {
            OBJ_RETAIN(compute);
            analytics_vals->compute_data = compute;
        }
        else {
            analytics_vals->compute_data = OBJ_NEW(opal_list_t);
            if (NULL == analytics_vals->compute_data) {
                ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);
                return NULL;
            }
        }
    }
    return analytics_vals;
}



int orcm_util_find_items(const char *keys[], int num_keys, opal_list_t *list,
               opal_value_t *items[], opal_bitmap_t *map)
{
    opal_value_t *kv;
    int i = 0;
    int j = 0;
    int num_found = 0;

    OPAL_LIST_FOREACH(kv, list, opal_value_t) {
        for (j = 0; j < num_keys; j++) {
            if (NULL == kv || NULL == kv->key) {
                return num_found;
            }
            if (!strcmp(kv->key, keys[j])) {
                num_found++;
                items[j] = kv;
                opal_bitmap_set_bit(map, i);

                break;
            }
        }
        i++;
    }

    return num_found;
}

/* create a hash key of 64bit for a given key.
 * This function is borrowed from the opal layer */
uint64_t orcm_util_create_hash_key(void *key, size_t key_size)
{
    uint64_t hash;
    const unsigned char *scanner;
    size_t index;

    hash = 0;
    scanner = (const unsigned char *)key;
    for (index = 0; index < key_size; index += 1) {
        hash = ORCM_UTIL_HASH_MULTIPLIER * hash + *scanner++;
    }
    return hash;
}

static orcm_analytics_value_t *orcm_util_retain_key_noncompute(opal_list_t *key,
                                                               opal_list_t *non_compute)
{
    orcm_analytics_value_t *analytics_vals = OBJ_NEW(orcm_analytics_value_t);

    if (NULL != analytics_vals) {
        if (NULL != key) {
            OBJ_RETAIN(key);
            analytics_vals->key = key;
        }
        if (NULL != non_compute) {
            OBJ_RETAIN(non_compute);
            analytics_vals->non_compute_data = non_compute;
        }
    }

    return analytics_vals;
}

int orcm_util_copy_list_items(opal_list_t *src, opal_list_t *dest)
{
    orcm_value_t *list_item = NULL;
    orcm_value_t *new_list_item = NULL;

    if (NULL == src || NULL == dest) {
        return ORCM_ERROR;
    }

    OPAL_LIST_FOREACH(list_item, src, orcm_value_t) {
        if (NULL == list_item) {
            return ORCM_ERROR;
        }
        new_list_item = orcm_util_copy_orcm_value(list_item);
        if (NULL == new_list_item) {
            return ORCM_ERR_OUT_OF_RESOURCE;
        }
        opal_list_append(dest, (opal_list_item_t *)new_list_item);
    }
    return ORCM_SUCCESS;
}

opal_list_t* orcm_util_copy_opal_list(opal_list_t *src)
{
    opal_list_t *dest = NULL;

    if (NULL != src) {
        dest = OBJ_NEW(opal_list_t);
        if (NULL == dest) {
            goto cleanup;
        }
        if (ORCM_SUCCESS != orcm_util_copy_list_items(src, dest)) {
            goto cleanup;
        }
        return dest;
    }

cleanup:
    if (NULL != dest) {
        OBJ_RELEASE(dest);
    }
    return NULL;
}

/* create an orcm_analytics_value that retains the key and non_compute data and
 * assign the input compute list to it. This function is not supposed to be used as a
 * general function that creates an orcm_analytics_value. It is used to pass the new
 * compute data from one plugin to the next one
 * */
orcm_analytics_value_t* orcm_util_load_orcm_analytics_value_compute(opal_list_t *key,
                                                            opal_list_t *non_compute,
                                                            opal_list_t *compute) {
    orcm_analytics_value_t *analytics_vals = orcm_util_retain_key_noncompute(key, non_compute);

    if (NULL != analytics_vals) {
        analytics_vals->compute_data = compute;
    }

    return analytics_vals;
}

static opal_list_t* orcm_util_update_analytics_time(opal_list_t* non_compute)
{
    opal_list_t* new_non_compute = OBJ_NEW(opal_list_t);
    orcm_value_t* list_item = NULL;
    orcm_value_t* new_list_item = NULL;
    struct timeval current_time;
    char* time_key = "ctime";

    gettimeofday(&current_time, NULL);

    if (NULL != new_non_compute) {
        if (NULL != non_compute) {
            OPAL_LIST_FOREACH(list_item, non_compute, orcm_value_t) {
                if (NULL == list_item) {
                    SAFE_RELEASE(new_non_compute);
                    return new_non_compute;
                }
                if (NULL == list_item->value.key ||
                    strncmp(list_item->value.key, time_key, strlen(time_key))) {
                    if (NULL != (new_list_item = orcm_util_copy_orcm_value(list_item))) {
                        opal_list_append(new_non_compute, (opal_list_item_t*)new_list_item);
                    } else {
                        SAFE_RELEASE(new_non_compute);
                        return new_non_compute;
                    }
                }
            }
        }

        if (ORCM_SUCCESS != (orcm_util_append_orcm_value(new_non_compute, time_key,
                                                         &current_time, OPAL_TIMEVAL, NULL))) {
            SAFE_RELEASE(new_non_compute);
        }
    }

    return new_non_compute;
}

/* create an orcm_analytics_value that retains the key, update the time in non_compute data to the
 * current time, and assign the compute data to it. This function is not supposed to be used as a
 * general function that creates an orcm_analytics_value. It is used to update the sample time to
 * the current time and pass the new compute data */
orcm_analytics_value_t* orcm_util_load_analytics_time_compute(opal_list_t* key,
                                                              opal_list_t* non_compute,
                                                              opal_list_t* compute)
{
    orcm_analytics_value_t* analytics_vals = OBJ_NEW(orcm_analytics_value_t);

    if (NULL != analytics_vals) {
        if (NULL != key) {
            OBJ_RETAIN(key);
            analytics_vals->key = key;
        }
        analytics_vals->non_compute_data = orcm_util_update_analytics_time(non_compute);
        analytics_vals->compute_data = compute;
    }

    return analytics_vals;
}

double orcm_util_get_number_orcm_value(orcm_value_t *source_value)
{
    double ret;

    if (NULL == source_value) {
        return 0.0;
    }

    switch(source_value->value.type) {
    case OPAL_INT:
        ret = (double)(source_value->value.data.integer);
        break;
    case OPAL_INT8:
        ret = (double)(source_value->value.data.int8);
        break;
    case OPAL_INT16:
        ret = (double)(source_value->value.data.int16);
        break;
    case OPAL_INT32:
        ret = (double)(source_value->value.data.int32);
        break;
    case OPAL_INT64:
        ret = (double)(source_value->value.data.int64);
        break;
    case OPAL_UINT:
        ret = (double)(source_value->value.data.uint);;
        break;
    case OPAL_UINT8:
        ret = (double)(source_value->value.data.uint8);
        break;
    case OPAL_UINT16:
        ret = (double)(source_value->value.data.uint16);
        break;
    case OPAL_UINT32:
        ret = (double)(source_value->value.data.uint32);
        break;
    case OPAL_UINT64:
        ret = (double)(source_value->value.data.int64);
        break;
    case OPAL_FLOAT:
        ret = (double)(source_value->value.data.fval);
        break;
    case OPAL_DOUBLE:
        ret = source_value->value.data.dval;
        break;
    default:
        ret = 0.0;
    }

    return ret;
}

double orcm_util_time_diff(struct timeval* time1, struct timeval* time2)
{
    double time1_d = (double)(time1->tv_sec) + (double)(time1->tv_usec) / 1E6;
    double time2_d = (double)(time2->tv_sec) + (double)(time2->tv_usec) / 1E6;

    return time2_d - time1_d;
}

static int get_seconds_multiplier(char type)
{
    switch(type)
    {
    case 'd':
        return 86400;
    case 'h':
        return 3600;
    case 'm':
        return 60;
    }
    return 1;
}

static int check_numeric(char* value) {
    int j = 0;
    if(NULL == value){
        return ORCM_ERROR;
    }
    while (value[j] != '\0') {
        if (!isdigit(value[j])) {
            return ORCM_ERR_BAD_PARAM;
        }
        j++;
    }
    return ORCM_SUCCESS;
}

uint64_t orcm_util_get_time_in_sec(char* time)
{
    char type;
    char* timeval = NULL;
    uint64_t seconds = 0;
    int multiplier = 1;
    int str_len = 0;
    if(NULL != time) {
        str_len = strlen(time);
        type = time[str_len - 1];
        if(isdigit(type)){
            timeval = strdup(time);
        } else {
            timeval = (char*)malloc(str_len);
            if(NULL != timeval){
                strncpy(timeval, time, str_len - 1);
                timeval[str_len-1] = '\0';
            }
        }
        multiplier = get_seconds_multiplier(type);
        if(ORCM_SUCCESS == check_numeric(timeval)){
            seconds = strtol(timeval, NULL, 0) * multiplier;
        }
    }
    SAFEFREE(timeval);
    return seconds;
}

void orcm_util_release_nested_orcm_value_list_item(orcm_value_t **item)
{
    if (NULL != item && NULL != *item){
        if (OPAL_PTR == (*item)->value.type){
            orcm_util_release_nested_orcm_value_list((opal_list_t *) (*item)->value.data.ptr);
            (*item)->value.data.ptr = NULL;
        }
        ORCM_RELEASE(*item);
    }
}

void orcm_util_release_nested_orcm_value_list(opal_list_t *list)
{
    if (NULL != list)  {
        orcm_value_t *item = NULL, *next = NULL;
        OPAL_LIST_FOREACH_SAFE(item, next, list, orcm_value_t){
            opal_list_remove_item(list, (opal_list_item_t *) item);
            orcm_util_release_nested_orcm_value_list_item(&item);
        }
        ORCM_RELEASE(list);
    }
}

void orcm_util_release_nested_orcm_cfgi_xml_parser_t_item(orcm_cfgi_xml_parser_t *item)
{
    if (NULL != item){
        orcm_util_release_nested_orcm_cfgi_xml_parser_t_list((opal_list_t *) &item->subvals);
        ORCM_RELEASE(item);
    }
}

void orcm_util_release_nested_orcm_cfgi_xml_parser_t_list(opal_list_t *list)
{
    if (NULL != list)  {
        orcm_cfgi_xml_parser_t *item = NULL, *next = NULL;
        OPAL_LIST_FOREACH_SAFE(item, next, list, orcm_cfgi_xml_parser_t){
            opal_list_remove_item(list, (opal_list_item_t *) item);
            orcm_util_release_nested_orcm_cfgi_xml_parser_t_item(item);
        }
    }
}