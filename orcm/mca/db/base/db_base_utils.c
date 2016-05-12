/*
 * Copyright (c) 2015 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/db/base/base.h"

#include "orcm/constants.h"

char* build_query_from_view_name_and_filters(const char* view_name, opal_list_t* filters);
char* get_opal_value_as_sql_string(opal_value_t *value);
char* timeval_to_iso8601(struct timeval* tv);
bool is_supported_opal_int_type(opal_data_type_t type);
int opal_value_to_orcm_db_item(const opal_value_t *kv,
                               orcm_db_item_t *item);
int orcm_util_find_items(const char *keys[], int num_keys, opal_list_t *list,
               opal_value_t *items[], opal_bitmap_t *map);
bool tv_to_str_time_stamp(const struct timeval *time, char *tbuf,
                          size_t size);
bool time_stamp_str_to_tv(char* stamp, struct timeval* time_value);
void tm_to_str_time_stamp(const struct tm *time, char *tbuf,
                          size_t size);

/* No need to include the entire header for one function. */
extern int asprintf(char** new_str, const char* format,...);

const char *opal_type_column_name = "data_type_id";
const char *value_column_names[] = {
    "value_str",
    "value_int",
    "value_real",
    NULL
};

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

char* timeval_to_iso8601(struct timeval* tv)
{ /* ISO8601: 'YYYY-MM-DDThh:mm:ss.nnn' */
    char* new_str = NULL;
    time_t tt;
    struct tm tms;
    int len = 24; /* strlen("YYYY-MM-DDThh:mm:ss.nnn") + 1; */
    char tmp[len+1];
    int milliseconds = (int)(tv->tv_usec / 1000);

    tt = tv->tv_sec;
    gmtime_r(&tt, &tms); /* use re-entrant version of gmtime; chosen so no localization occurs */
    strftime(tmp, len, "%FT%T", &tms);
    asprintf(&new_str, "%s.%03d", tmp, milliseconds);

    return new_str;
}

char* get_opal_value_as_sql_string(opal_value_t *value)
{
    char* new_str = NULL;
    if(NULL != value) {
        switch(value->type) {
        case OPAL_STRING:
            new_str = strdup(value->data.string);
            break;
        case OPAL_INT:
            asprintf(&new_str, "%d", value->data.integer);
            break;
        case OPAL_INT8:
            asprintf(&new_str, "%d", value->data.int8);
            break;
        case OPAL_INT16:
            asprintf(&new_str, "%d", value->data.int16);
            break;
        case OPAL_INT32:
            asprintf(&new_str, "%d", value->data.int32);
            break;
        case OPAL_INT64:
            asprintf(&new_str, "%ld", value->data.int64);
            break;
        case OPAL_UINT:
            asprintf(&new_str, "%d", value->data.uint);
            break;
        case OPAL_UINT8:
            asprintf(&new_str, "%d", value->data.uint8);
            break;
        case OPAL_UINT16:
            asprintf(&new_str, "%d", value->data.uint16);
            break;
        case OPAL_UINT32:
            asprintf(&new_str, "%d", value->data.uint32);
            break;
        case OPAL_UINT64:
            asprintf(&new_str, "%ld", value->data.uint64);
            break;
        case OPAL_FLOAT:
            asprintf(&new_str, "%f", value->data.fval);
            break;
        case OPAL_DOUBLE:
            asprintf(&new_str, "%f", value->data.dval);
            break;
        case OPAL_TIMEVAL:
        case OPAL_TIME:
            new_str = timeval_to_iso8601(&value->data.tv);
            break;
        default: /* Unsupported opal type; leave new_str == NULL */
            break;
        }
    }
    return new_str;
}

static char *op_strings[] = {
    NULL,
    " < ",
    " > ",
    " = ",
    " <> ",
    " <= ",
    " >= ",
    " like ",
    " like ",
    " like ",
    " in "
};

static bool add_where_clauses(char **original_query, orcm_db_filter_t *filter, bool *first_clause)
{
    if(NULL == filter || filter->op == NONE || NULL == first_clause || NULL == original_query || NULL == *original_query) {
        return false;
    } else {
        char* old_query = *original_query;
        char c1 = (IN != filter->op)?'\'':'(';
        char c2 = (IN != filter->op)?'\'':')';
        char* val = get_opal_value_as_sql_string(&filter->value);
        char* and = (char*)((*first_clause)?" where ":" and ");

        *first_clause = false; /* we already used this so mark it as */

        asprintf(original_query, "%s%s%s%s%c%s%c", old_query, and, filter->value.key, op_strings[(int)filter->op], c1, val, c2);

        free(val);
        free(old_query);

        return true;
    }
}

char* build_query_from_view_name_and_filters(const char* view_name, opal_list_t* filters)
{
    char* query = NULL;

    if(NULL != view_name && 0 < strlen(view_name)) {
        if(NULL == filters || 0 == opal_list_get_size(filters)) {
            asprintf(&query, "select * from %s;", view_name);
        } else {
            orcm_db_filter_t* filter = NULL;
            char* old_query = NULL;
            bool first_clause = true;

            asprintf(&query, "select * from %s", view_name);

            OPAL_LIST_FOREACH(filter, filters, orcm_db_filter_t) {
                if(false == add_where_clauses(&query, filter, &first_clause)) {
                    free(query);
                    return NULL;
                }
            }
            old_query = query;
            asprintf(&query, "%s LIMIT 10000000;", old_query);
            free(old_query);
        }
    }
    return query;
}

bool is_supported_opal_int_type(opal_data_type_t type)
{
    if(OPAL_BYTE == type || OPAL_BOOL == type || OPAL_SIZE == type || OPAL_PID == type ||
       OPAL_INT == type || OPAL_INT8 == type || OPAL_INT16 == type || OPAL_INT32 == type || OPAL_INT64 == type ||
       OPAL_UINT == type || OPAL_UINT8 == type || OPAL_UINT16 == type || OPAL_UINT32 == type || OPAL_UINT64 == type) {
        return true;
    } else {
        return false;
    }
}


bool tv_to_str_time_stamp(const struct timeval *time, char *tbuf,
                          size_t size)
{
    struct timeval nrm_time = *time;
    struct tm *tm_info;
    char date_time[30];
    char fraction[10];

    /* Normalize */
    while (nrm_time.tv_usec < 0) {
        nrm_time.tv_usec += 1000000;
        nrm_time.tv_sec--;
    }
    while (nrm_time.tv_usec >= 1000000) {
        nrm_time.tv_usec -= 1000000;
        nrm_time.tv_sec++;
    }

    /* Print in format: YYYY-MM-DD HH:MM:SS.fraction time zone */
    tm_info = localtime(&nrm_time.tv_sec);
    if (NULL != tm_info) {
        strftime(date_time, sizeof(date_time), "%F %T", tm_info);
        snprintf(fraction, sizeof(fraction), "%f",
                 (float)(time->tv_usec / 1000000.0));
        snprintf(tbuf, size, "%s%s", date_time, fraction + 1);
        return true;
    } else {
        return false;
    }
}

bool time_stamp_str_to_tv(char* stamp, struct timeval* time_value)
{
    char buffer[5]; // largest component is year 4 + 1 for '\0';
    struct tm t;

    if(NULL == stamp || NULL == time_value) {
        return false;
    }
    memset(&t, 0, sizeof(struct tm));

    strncpy(buffer, stamp, 4); /* get year */
    buffer[4] = '\0';
    t.tm_year = atoi(buffer) - 1900;

    strncpy(buffer, &stamp[5], 2); /* get month */
    buffer[2] = '\0';
    t.tm_mon = atoi(buffer) - 1;

    strncpy(buffer, &stamp[8], 2); /* get day of month */
    buffer[2] = '\0';
    t.tm_mday = atoi(buffer);

    strncpy(buffer, &stamp[11], 2); /* get hour of day */
    buffer[2] = '\0';
    t.tm_hour = atoi(buffer);

    strncpy(buffer, &stamp[14], 2); /* get minute of the hour */
    buffer[2] = '\0';
    t.tm_min = atoi(buffer);

    strncpy(buffer, &stamp[17], 2); /* get seconds of the minute */
    buffer[2] = '\0';
    t.tm_sec = atoi(buffer);

    if(23 <= strlen(stamp)) {
        strncpy(buffer, &stamp[20], 3); /* get microseconds (resolution is milliseconds) of the second */
        buffer[3] = '\0';
        time_value->tv_usec = atoi(buffer) * 1000;
    } else {
        time_value->tv_usec = 0;
    }
    time_value->tv_sec = mktime(&t);

    return true;
}

void tm_to_str_time_stamp(const struct tm *time, char *tbuf,
                          size_t size)
{
    strftime(tbuf, size, "%F %T", time);
}
