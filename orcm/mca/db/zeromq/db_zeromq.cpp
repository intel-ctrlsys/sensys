/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 *
 */

#include "orcm_config.h"
#include "orcm/constants.h"

#include <time.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "opal/class/opal_pointer_array.h"
#include "opal/util/argv.h"
#include "opal/util/output.h"
#include "opal_stdint.h"

#include "orcm/runtime/orcm_globals.h"

#include "orcm/mca/db/base/base.h"
#include "db_zeromq.h"

#include "ZeroMQPublisher.hpp"
#include <memory>
#include <iostream>
#include <sstream>
#include <map>
#include <chrono>
#include <thread>

using std::cout;
using std::endl;

std::shared_ptr<Publisher> orcm_db_zeromq_object(new ZeroMQPublisher());

#define ZMQ_THREADS     (4)
#define MAX_ZMQ_BUFFERS (10000)

BEGIN_C_DECLS
#include "orcm/util/utils.h"

/* Module API functions */
int orcm_db_zeromq_init(struct orcm_db_base_module_t* imod);
void orcm_db_zeromq_finalize(struct orcm_db_base_module_t* imod);
int orcm_db_zeromq_store_new(struct orcm_db_base_module_t* imod,
                             orcm_db_data_type_t data_type,
                             opal_list_t* kvs,
                             opal_list_t* ret);
void orcm_db_zeromq_output_callback(int level, const char* msg);
void orcm_db_zeromq_print_value(const opal_value_t *kv, std::stringstream& ss);
std::string orcm_db_zeromq_print_orcm_json_format(opal_list_t *kvs, bool inventory = false);
void orcm_db_zeromq_print_time_value(const struct timeval *time, std::stringstream& ss);

mca_db_zeromq_module_t mca_db_zeromq_module = {
    {
        orcm_db_zeromq_init,
        orcm_db_zeromq_finalize,
        NULL,
        orcm_db_zeromq_store_new,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL
    },
};

int orcm_db_zeromq_init(struct orcm_db_base_module_t* imod)
{
    mca_db_zeromq_module_t* mod = (mca_db_zeromq_module_t*)imod;
    try {
        orcm_db_zeromq_object->Init(mod->bind_port, ZMQ_THREADS, MAX_ZMQ_BUFFERS, orcm_db_zeromq_output_callback);
        orcm_db_zeromq_output_callback(1, "mca: db: zeromq: selected and initialized.");
    } catch (ZeroMQException& ex) {
        std::stringstream ss;
        ss << "mca: db: zeromq: selected but failed to initialize: '" << ex.what() <<"' (" << ex.ZMQError() << ")";
        orcm_db_zeromq_output_callback(0, ss.str().c_str());
        return ORCM_ERROR;
    }

    return ORCM_SUCCESS;
}

void orcm_db_zeromq_finalize(struct orcm_db_base_module_t* imod)
{
    orcm_db_zeromq_object->Finalize();
    orcm_db_zeromq_output_callback(1, "mca: db: zeromq: finalized.");
}

int orcm_db_zeromq_store_new(struct orcm_db_base_module_t* imod,
                             orcm_db_data_type_t data_type,
                             opal_list_t* kvs,
                             opal_list_t* ret)
{
    static bool firstMessage = true;
    static std::map<orcm_db_data_type_t,std::string> lookup = {
        { ORCM_DB_ENV_DATA, "monitoring_data" },
        { ORCM_DB_EVENT_DATA, "monitoring_event" },
        { ORCM_DB_INVENTORY_DATA, "monitoring_inventory" },
        { ORCM_DB_DIAG_DATA, "diagnostic_data"}
    };
    mca_db_zeromq_module_t* mod = (mca_db_zeromq_module_t*)imod;
    std::string json = orcm_db_zeromq_print_orcm_json_format(kvs, (ORCM_DB_INVENTORY_DATA == data_type));

    try {
        if(firstMessage) {
            std::this_thread::sleep_for(std::chrono::milliseconds(750));
            firstMessage = false;
        }
        orcm_db_zeromq_object->PublishMessage(lookup[data_type], json);
        orcm_db_zeromq_output_callback(9, "mca: db: zeromq: published message.");
    } catch (ZeroMQException& ex) {
        std::stringstream ss;
        ss << "mca: db: zeromq: public message failed: '" << ex.what() << "' (" << ex.ZMQError() << ")";
        orcm_db_zeromq_output_callback(0, ss.str().c_str());
        return ORCM_ERROR;
    } catch (std::bad_alloc& ex) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }

    return ORCM_SUCCESS;
}

void orcm_db_zeromq_output_callback(int level, const char* msg)
{
    opal_output_verbose(level, orcm_db_base_framework.framework_output, msg);
}

std::string orcm_db_zeromq_print_orcm_json_format(opal_list_t *kvs, bool inventory)
{
    std::stringstream ss;
    bool insert_comma = false;
    orcm_value_t* kv;
    ss << "{\"data\":[";
    if (nullptr != kvs) {
        OPAL_LIST_FOREACH(kv, kvs, orcm_value_t) {
            if (true == insert_comma) {
                ss << ",";
            }
            ss << "{\"key\":\"" << kv->value.key << "\",\"value\":";
            orcm_db_zeromq_print_value(&(kv->value), ss);

            if (!inventory && NULL != kv->units && '\0' != *kv->units) {
                ss << ",\"units\":\"" << kv->units << "\"}";
            }
            else
                ss << "}";
            insert_comma = true;
        }
    }
    ss << "]}";
    return ss.str();
}


void orcm_db_zeromq_print_value(const opal_value_t *kv, std::stringstream &ss)
{
    switch (kv->type) {
    case OPAL_STRING:
        ss << "\"" << kv->data.string << "\"";
        break;
    case OPAL_SIZE:
        ss << kv->data.size;
        break;
    case OPAL_INT:
        ss << kv->data.integer;
        break;
    case OPAL_INT8:
        ss << kv->data.int8;
        break;
    case OPAL_INT16:
        ss << kv->data.int16;
        break;
    case OPAL_INT32:
        ss << kv->data.int32;
        break;
    case OPAL_INT64:
        ss << kv->data.int64;
        break;
    case OPAL_UINT:
        ss << kv->data.uint;
        break;
    case OPAL_UINT8:
        ss << kv->data.uint8;
        break;
    case OPAL_UINT16:
        ss << kv->data.uint16;
        break;
    case OPAL_UINT32:
        ss << kv->data.uint32;
        break;
    case OPAL_UINT64:
        ss << kv->data.uint64;
        break;
    case OPAL_PID:
        ss << (unsigned long)kv->data.pid;
        break;
    case OPAL_BOOL:
        ss << (kv->data.flag ? "true" : "false");
        break;
    case OPAL_FLOAT:
        ss <<  kv->data.fval;
        break;
    case OPAL_DOUBLE:
        ss << kv->data.dval;
        break;
    case OPAL_TIMEVAL:
        orcm_db_zeromq_print_time_value(&kv->data.tv, ss);
        break;
    default:
        ss << "\"\"";
        break;
    }
}

void orcm_db_zeromq_print_time_value(const struct timeval *time, std::stringstream& ss)
{
    using std::string;
    struct tm tm_info;
    char date_time[30];
    char fraction[10];
    char time_zone[10];

    // Print in format: YYYY-MM-DD HH:MM:SS.fraction time zone
    localtime_r(&time->tv_sec, &tm_info);
    strftime(date_time, sizeof(date_time), "%F %T", &tm_info);
    strftime(time_zone, sizeof(time_zone), "%z", &tm_info);
    snprintf(fraction, sizeof(fraction), "%.3f",
             (float)(time->tv_usec / 1000000.0));
    ss << "\"" << string(date_time) << string(fraction + 1) << string(time_zone) << "\"";
}

END_C_DECLS
