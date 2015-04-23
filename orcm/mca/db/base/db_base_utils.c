/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/db/base/base.h"

int opal_value_to_orcm_db_item(const opal_value_t *kv,
                               orcm_db_item_t *item)
{
    item->opal_type = kv->type;

    switch (kv->type) {
    case OPAL_STRING:
        item->value.value_str = kv->data.string;
        item->item_type = ORCM_DB_ITEM_STRING;
        break;
    case OPAL_SIZE:
        item->value.value_int = (long long int)kv->data.size;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_INT:
        item->value.value_int = (long long int)kv->data.integer;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_INT8:
        item->value.value_int = (long long int)kv->data.int8;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_INT16:
        item->value.value_int = (long long int)kv->data.int16;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_INT32:
        item->value.value_int = (long long int)kv->data.int32;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_INT64:
        item->value.value_int = (long long int)kv->data.int64;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_UINT:
        item->value.value_int = (long long int)kv->data.uint;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_UINT8:
        item->value.value_int = (long long int)kv->data.uint8;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_UINT16:
        item->value.value_int = (long long int)kv->data.uint16;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_UINT32:
        item->value.value_int = (long long int)kv->data.uint32;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_UINT64:
        item->value.value_int = (long long int)kv->data.uint64;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_PID:
        item->value.value_int = (long long int)kv->data.pid;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_BOOL:
        item->value.value_int = (long long int)kv->data.flag;
        item->item_type = ORCM_DB_ITEM_INTEGER;
        break;
    case OPAL_FLOAT:
        item->value.value_real = (double)kv->data.fval;
        item->item_type = ORCM_DB_ITEM_REAL;
        break;
    case OPAL_DOUBLE:
        item->value.value_real = kv->data.dval;
        item->item_type = ORCM_DB_ITEM_REAL;
        break;
    default:
        return ORCM_ERR_NOT_SUPPORTED;
    }

    return ORCM_SUCCESS;
}
