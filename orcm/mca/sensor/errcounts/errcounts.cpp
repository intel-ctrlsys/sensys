/*
 * Copyright (c) 2015-2016  Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

extern "C" {
    // OPAL
    #include "opal/runtime/opal_progress_threads.h"
    #include "opal/class/opal_list.h"
    // ORTE
    #include "orte/util/proc_info.h"
    #include "orte/util/show_help.h"
    // ORCM
    #include "orcm/runtime/orcm_globals.h"
    #include "orcm/mca/analytics/analytics.h"
    #include "orcm/mca/sensor/base/sensor_private.h"
    #include "orcm/mca/db/db.h"

    extern orcm_value_t* orcm_util_load_orcm_value(char *key, void *data, opal_data_type_t type, char* units);
    extern orcm_analytics_value_t* orcm_util_load_orcm_analytics_value(opal_list_t *key, opal_list_t *non_compute, opal_list_t *compute);

    extern orcm_sensor_base_t orcm_sensor_base;
    extern orte_proc_info_t orte_process_info;
}
#include "orcm/mca/sensor/base/sensor_runtime_metrics.h"

// Plugin Includes...
#include "sensor_errcounts.h"
#include "errcounts.h"
#include "edac_collector.h"

// Conveniences...
#define SAFE_DELETE(x) if(NULL != x){ delete x;x=NULL;}
#define SAFE_FREE(x) if(NULL != x){ free((void*)x);x=NULL;}
#define ON_NULL_BREAK(x) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);break;}
#define ON_NULL_RETURN(x) if(NULL==x){ORTE_ERROR_LOG(ORCM_ERR_OUT_OF_RESOURCE);return;}
#define ON_FALSE_BREAK(x) if(!x){break;}
#define ON_FALSE_RETURN_BOOL(x) if(!x){return x;}
#define ON_FAILURE_RETURN_FALSE(x) if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);return false;}
#define ON_FAILURE_BREAK(x) if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);break;}
#define ON_FAILURE_RETURN(x) if(ORCM_SUCCESS!=x){ORTE_ERROR_LOG(x);return;}
#define NOT_NULL ((void*)0xffffffffffffffff)
// OBJ_RELEASE only conditially sets the pointer to NULL
#define SAFE_OBJ_RELEASE(x) if(NULL!=x) { OBJ_RELEASE(x); x=NULL; }

// Static constants...
const std::string errcounts_impl::plugin_name_ = "errcounts";

// C++
#include <iostream>
#include <sstream>
#include <iomanip>
#include <new>

// Class Implementation
using namespace std;

errcounts_impl::errcounts_impl()
 : collector_(NULL), ev_paused_(false), ev_base_(NULL), errcounts_sampler_(NULL),
   edac_missing_(false), collect_metrics_(NULL), diagnostics_(0)
{
    collector_ = new edac_collector(error_callback_relay, mca_sensor_errcounts_component.edac_mc_folder);
}

errcounts_impl::~errcounts_impl()
{
    finalize();
    SAFE_DELETE(collector_);
}

int errcounts_impl::init(void)
{
    try {
        collect_metrics_ = new RuntimeMetrics("errcounts", orcm_sensor_base.collect_metrics,
                                              mca_sensor_errcounts_component.collect_metrics);
    } catch(bad_alloc&) {
        return ORCM_ERR_OUT_OF_RESOURCE;
    }
    if(0 == mca_sensor_errcounts_component.sample_rate) {
        mca_sensor_errcounts_component.sample_rate = orcm_sensor_base.sample_rate;
    }
    hostname_ = orcm_sensor_base.host_tag_value;
    edac_missing_ = (collector_->have_edac())?false:true;
    if(true == edac_missing_ || 0 == collector_->get_mc_folder_count()) {
        orte_show_help("help-orcm-sensor-errcounts.txt", "no-edac", true, (char*)hostname_.c_str());
        return ORCM_ERROR;
    }
    return ORCM_SUCCESS;
}

void errcounts_impl::finalize(void)
{
    if(true == edac_missing_) {
        return;
    }
    stop(0);
    ev_destroy_thread();

    SAFE_DELETE(collect_metrics_);
    edac_missing_ = true;
}

void errcounts_impl::start(orte_jobid_t job)
{
    if(-999 == (int)job) {
        return; // NOOP ID; Succeed without actually starting...
    }
    if(true == edac_missing_) {
        return;
    }
    // start a separate errcounts progress thread for sampling
    if (true == mca_sensor_errcounts_component.use_progress_thread) {
        // setup errcounts sampler
        errcounts_sampler_ = OBJ_NEW(orcm_sensor_sampler_t);

        // Create event thread
        ev_create_thread();
        if (NULL == ev_base_) {
            OBJ_RELEASE(errcounts_sampler_);
        }
    }

    return;
}

void errcounts_impl::stop(orte_jobid_t job)
{
    if(-999 == (int)job) {
        return; // NOOP ID; Succeed without actually stopping...
    }
    if(true == edac_missing_) {
        return;
    }
    ev_pause();
    if(NULL != errcounts_sampler_) {
        OBJ_RELEASE(errcounts_sampler_);
    }
}

void errcounts_impl::sample(orcm_sensor_sampler_t* sampler)
{
    if(true == edac_missing_) {
        return;
    }
    if(NULL == sampler) {
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        return;
    }
    if (false == mca_sensor_errcounts_component.use_progress_thread) {
        errcounts_sampler_ = sampler;
        collect_sample();
        errcounts_sampler_ = NULL;
    }
}

void errcounts_impl::log(opal_buffer_t* buf)
{
    if(true == edac_missing_) {
        return;
    }
    if(NULL == buf) {
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        return;
    }
    opal_list_t* non_compute = NULL;
    opal_list_t* key = NULL;
    orcm_analytics_value_t* analytics_vals = NULL;
    log_samples_labels_.clear();
    log_samples_values_.clear();
    while(true) {
        string hostname;
        struct timeval timestamp;

        // Unpack data...
        ON_FALSE_BREAK(unpack_string(buf, hostname));
        ON_FALSE_BREAK(unpack_timestamp(buf, timestamp));
        ON_FALSE_BREAK(unpack_data_sample(buf));

        // Create lists...
        key = OBJ_NEW(opal_list_t);
        ON_NULL_BREAK(key);

        non_compute = OBJ_NEW(opal_list_t);
        ON_NULL_BREAK(non_compute);

        // Store data for analytics...
        orcm_value_t* value = NULL;

        // load the timestamp
        value = orcm_util_load_orcm_value((char*)"ctime", (void*)&timestamp, OPAL_TIMEVAL, NULL);
        ON_NULL_BREAK(value);
        opal_list_append(non_compute, (opal_list_item_t*)value);

        // load the node name
        value = orcm_util_load_orcm_value((char*)"hostname", (void*)hostname.c_str(), OPAL_STRING, NULL);
        ON_NULL_BREAK(value);
        opal_list_append(key, (opal_list_item_t*)value);

        // load the data_group (pluging name)
        value = orcm_util_load_orcm_value((char*)"data_group", (void*)plugin_name_.c_str(), OPAL_STRING, NULL);
        ON_NULL_BREAK(value);
        opal_list_append(key, (opal_list_item_t*)value);

        analytics_vals = orcm_util_load_orcm_analytics_value(key, non_compute, NULL);
        ON_NULL_BREAK(analytics_vals);
        ON_NULL_BREAK(analytics_vals->key);
        ON_NULL_BREAK(analytics_vals->non_compute_data);
        ON_NULL_BREAK(analytics_vals->compute_data);
        for(size_t i = 0; i < log_samples_labels_.size(); ++i) {
            value = orcm_util_load_orcm_value((char*)log_samples_labels_[i].c_str(), (void*)&log_samples_values_[i], OPAL_INT32, NULL);
            ON_NULL_BREAK(value);

            opal_list_append(analytics_vals->compute_data, (opal_list_item_t *)value);
        }
        orcm_analytics.send_data(analytics_vals);
        break; // execute while() only once...
    }
    SAFE_OBJ_RELEASE(key);
    SAFE_OBJ_RELEASE(non_compute);
    SAFE_OBJ_RELEASE(analytics_vals);
}


void errcounts_impl::inventory_collect(opal_buffer_t* inventory_snapshot)
{
    if(mca_sensor_errcounts_component.test) {
        generate_inventory_test_vector(inventory_snapshot);
        return;
    }
    if(true == edac_missing_) {
        return;
    }
    if(NULL == inventory_snapshot) {
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        return;
    }

    inv_samples_.clear();
    collector_->collect_inventory(inventory_callback_relay, this);

    while(true) {
        ON_FALSE_BREAK(pack_string(inventory_snapshot, plugin_name_));
        ON_FALSE_BREAK(pack_string(inventory_snapshot, hostname_));
        pack_inv_sample(inventory_snapshot);
        break;
    }
}

void errcounts_impl::my_inventory_log_cleanup(int dbhandle, int status, opal_list_t *kvs, opal_list_t *output, void *cbdata)
{
    SAFE_OBJ_RELEASE(kvs);
}

void errcounts_impl::inventory_log(char* hostname, opal_buffer_t* inventory_snapshot)
{
    if(true == edac_missing_) {
        return;
    }
    if(NULL == hostname || NULL == inventory_snapshot) {
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        return;
    }
    opal_list_t* records = OBJ_NEW(opal_list_t);
    while(true) {
        ON_NULL_BREAK(records);
        string phostname;
        struct timeval timestamp;
        gettimeofday(&timestamp, NULL);
        ON_FALSE_BREAK(unpack_string(inventory_snapshot, phostname));
        orcm_value_t* item = make_orcm_value_string("hostname", phostname.c_str());
        ON_NULL_BREAK(item);
        opal_list_append(records, (opal_list_item_t*)item);

        item = orcm_util_load_orcm_value((char*)"ctime", (void*)&timestamp, OPAL_TIMEVAL, NULL);
        ON_NULL_BREAK(item);
        opal_list_append(records, (opal_list_item_t*)item);

        ON_FALSE_BREAK(unpack_inv_sample(inventory_snapshot));
        void* failed = NOT_NULL;
        for(map<string,string>::iterator it = inv_log_samples_.begin(); it != inv_log_samples_.end(); ++it) {
            item = make_orcm_value_string(it->first.c_str(), it->second.c_str());
            if(NULL == item) {
                failed = NULL;
            }
            ON_NULL_BREAK(item);
            opal_list_append(records, (opal_list_item_t*)item);
        }
        ON_NULL_BREAK(failed);

        if (0 <= orcm_sensor_base.dbhandle) {
            orcm_db.store_new(orcm_sensor_base.dbhandle, ORCM_DB_INVENTORY_DATA, records, NULL, my_inventory_log_cleanup, NULL);
        } else {
            my_inventory_log_cleanup(-1, -1, records, NULL, NULL);
        }
        records = NULL;
        break; // execute while() only once...
    }
    SAFE_OBJ_RELEASE(records);
}

void errcounts_impl::set_sample_rate(int sample_rate)
{
    if(true == edac_missing_) {
        return;
    }
    // set the errcounts sample rate if seperate thread is enabled
    if (true == mca_sensor_errcounts_component.use_progress_thread) {
        mca_sensor_errcounts_component.sample_rate = sample_rate;
    } else {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor errcounts : set_sample_rate: called but not using per-thread sampling",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }
}

void errcounts_impl::get_sample_rate(int* sample_rate)
{
    if(true == edac_missing_) {
        return;
    }
    if(NULL == sample_rate) {
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        return;
    }
    *sample_rate = mca_sensor_errcounts_component.sample_rate;
    if (false == mca_sensor_errcounts_component.use_progress_thread) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor errcounts : get_sample_rate: called but not using per-thread sampling",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }
}

int errcounts_impl::enable_sampling(const char* sensor_spec)
{
    return collect_metrics_->SetCollectionState(true, sensor_spec);
}

int errcounts_impl::disable_sampling(const char* sensor_spec)
{
    return collect_metrics_->SetCollectionState(false, sensor_spec);
}

int errcounts_impl::reset_sampling(const char* sensor_spec)
{
    return collect_metrics_->ResetCollectionState(sensor_spec);
}


// Callback Relay Handling
void errcounts_impl::error_callback_relay(const char* pathname, int error_number, void* user_data)
{
    if(NULL != user_data) {
        ((errcounts_impl*)user_data)->error_callback(pathname, error_number);
    }
}

void errcounts_impl::data_callback_relay(const char* label, int error_count, void* user_data)
{
    if(NULL != user_data) {
        ((errcounts_impl*)user_data)->data_callback(label, error_count);
    }
}

void errcounts_impl::inventory_callback_relay(const char* label, const char* name, void* user_data)
{
    if(NULL != user_data) {
        ((errcounts_impl*)user_data)->inventory_callback(label, name);
    }
}

void errcounts_impl::perthread_errcounts_sample_relay(int fd, short args, void *user_data)
{
    (void)fd;
    (void)args;
    if(NULL != user_data) {
        ((errcounts_impl*)user_data)->perthread_errcounts_sample();
    }
}


// In object callbacks from relays...
void errcounts_impl::error_callback(const char* pathname, int error_number)
{
    if(NULL == pathname) {
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        return;
    }
    stringstream ss;
    ss << "WARNING: Trouble accessing sysfs entry (errno=" << error_number << "): " << pathname << ": Skipping this data entry.";
    opal_output_verbose(1, orcm_sensor_base_framework.framework_output,
                        "WARNING: %s sensor errcounts : sample: trouble accessing sysfs entry (errno=%d): '%s': skipping data entry",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), error_number, pathname);
}

void errcounts_impl::data_callback(const char* label, int error_count)
{
    if(NULL == label) {
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        return;
    }
    if(NULL == collect_metrics_ || collect_metrics_->DoCollectMetrics(label)) {
        data_samples_labels_.push_back(string(label));
        data_samples_values_.push_back((int32_t)error_count);
    }
}

void errcounts_impl::inventory_callback(const char* label, const char* name)
{
    if(NULL == label || NULL == name) {
        ORTE_ERROR_LOG(ORTE_ERR_BAD_PARAM);
        return;
    }
    inv_samples_[string(label)] = string(name);
    if(NULL != collect_metrics_) {
        collect_metrics_->TrackSensorLabel(name);
    }
}

void errcounts_impl::OrcmSensorXfer(opal_buffer_t* buffer)
{
    ORCM_SENSOR_XFER(buffer);
}

void errcounts_impl::OpalEventEvtimerAdd(opal_event_t* ev, struct timeval* tv)
{
    opal_event_evtimer_add(ev, tv);
}

void errcounts_impl::perthread_errcounts_sample()
{
    collect_sample(true);

    OrcmSensorXfer(&errcounts_sampler_->bucket);

    // clear the bucket
    OBJ_DESTRUCT(&errcounts_sampler_->bucket);
    OBJ_CONSTRUCT(&errcounts_sampler_->bucket, opal_buffer_t);

    // check if errcounts sample rate is provided for this
    if (mca_sensor_errcounts_component.sample_rate != errcounts_sampler_->rate.tv_sec) {
        errcounts_sampler_->rate.tv_sec = mca_sensor_errcounts_component.sample_rate;
    }
    // set ourselves to sample again
    OpalEventEvtimerAdd(&errcounts_sampler_->ev, &errcounts_sampler_->rate);
}


// Common data collection...
void errcounts_impl::collect_sample(bool perthread /* = false*/)
{
    if(mca_sensor_errcounts_component.test) {
        generate_test_samples(perthread);
        return;
    }

    if(0 == collect_metrics_->CountOfCollectedLabels() && !collect_metrics_->DoCollectMetrics()) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor errcounts : skipping actual sample collection",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
        return;
    }
    diagnostics_ |= 0x1;

    if(true == perthread) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor errcounts : perthread_errcounts_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    } else {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor errcounts : errcounts_sample: called",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    }
    data_samples_labels_.clear();
    data_samples_values_.clear();
    collector_->collect_data(data_callback_relay, this);

    opal_buffer_t buffer;
    OBJ_CONSTRUCT(&buffer, opal_buffer_t);
    while(data_samples_values_.size() > 0) {
        ON_FALSE_BREAK(pack_string(&buffer, plugin_name_));
        ON_FALSE_BREAK(pack_string(&buffer, hostname_));
        ON_FALSE_BREAK(pack_timestamp(&buffer));
        ON_FALSE_BREAK(pack_data_sample(&buffer));

        opal_buffer_t* bptr = &buffer;
        int rc = opal_dss.pack(&errcounts_sampler_->bucket, &bptr, 1, OPAL_BUFFER);
        ON_FAILURE_BREAK(rc);
        break;
    }
    OBJ_DESTRUCT(&buffer);
}


// Helper implementations...
orcm_value_t* errcounts_impl::make_orcm_value_string(const char* name, const char* value)
{
    orcm_value_t* rv = OBJ_NEW(orcm_value_t);
    if(NULL != rv) {
        rv->value.type = OPAL_STRING;
        rv->value.key = strdup(name);
        rv->value.data.string = strdup(value);
    }
    return rv;
}

void errcounts_impl::ev_pause()
{
    if(NULL != ev_base_ && false == ev_paused_) {
        if(OPAL_SUCCESS == opal_progress_thread_pause("errcounts")) {
            ev_paused_ = true;
        }
    }
}

void errcounts_impl::ev_resume()
{
    if(NULL != ev_base_ && true == ev_paused_) {
        if(OPAL_SUCCESS == opal_progress_thread_resume("errcounts")) {
            ev_paused_ = false;
        }
    }
}

void errcounts_impl::ev_create_thread()
{
    if(NULL == ev_base_ && NULL != errcounts_sampler_) {
        if (NULL == (ev_base_ = opal_progress_thread_init("errcounts"))) {
            ORTE_ERROR_LOG(ORCM_ERROR);
            return;
        }
        errcounts_sampler_->rate.tv_sec = mca_sensor_errcounts_component.sample_rate;
        errcounts_sampler_->log_data = orcm_sensor_base.log_samples;
        opal_event_evtimer_set(ev_base_, &errcounts_sampler_->ev,
                               perthread_errcounts_sample_relay, this);
        opal_event_evtimer_add(&errcounts_sampler_->ev, &errcounts_sampler_->rate);
        ev_paused_ = false;
    }
}

void errcounts_impl::ev_destroy_thread()
{
    if(NULL != ev_base_) {
        opal_progress_thread_finalize("errcounts");
        ev_base_ = NULL;
        ev_paused_ = false;
    }
}


// Packing methods...
bool errcounts_impl::pack_string(opal_buffer_t* buffer, const std::string& str) const
{
    const char* str_ptr = str.c_str();
    int rc = opal_dss.pack(buffer, &str_ptr, 1, OPAL_STRING);
    ON_FAILURE_RETURN_FALSE(rc);
    return true;
}

bool errcounts_impl::pack_int32(opal_buffer_t* buffer, int32_t value) const
{
    int rc = opal_dss.pack(buffer, &value, 1, OPAL_INT32);
    ON_FAILURE_RETURN_FALSE(rc);
    return true;
}

bool errcounts_impl::pack_timestamp(opal_buffer_t* buffer) const
{
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    int rc = opal_dss.pack(buffer, &current_time, 1, OPAL_TIMEVAL);
    ON_FAILURE_RETURN_FALSE(rc);
    return true;
}

bool errcounts_impl::pack_data_sample(opal_buffer_t* buffer)
{
    ON_FALSE_RETURN_BOOL(pack_int32(buffer, (int32_t)data_samples_labels_.size()));
    for(size_t i = 0; i < data_samples_labels_.size(); ++i) {
        ON_FALSE_RETURN_BOOL(pack_string(buffer, data_samples_labels_[i]));
        ON_FALSE_RETURN_BOOL(pack_int32(buffer, data_samples_values_[i]));
    }
    return true;
}

bool errcounts_impl::pack_inv_sample(opal_buffer_t* buffer)
{
    ON_FALSE_RETURN_BOOL(pack_int32(buffer, (int32_t)inv_samples_.size()));
    for(map<string,string>::iterator it = inv_samples_.begin(); it != inv_samples_.end(); ++it) {
        ON_FALSE_RETURN_BOOL(pack_string(buffer, it->first));
        ON_FALSE_RETURN_BOOL(pack_string(buffer, it->second));
    }
    return true;
}

bool errcounts_impl::unpack_string(opal_buffer_t* buffer, std::string& str) const
{
    bool rv = true;
    int32_t n = 1;
    char* str_ptr = NULL;
    int rc = opal_dss.unpack(buffer, &str_ptr, &n, OPAL_STRING);
    ON_FAILURE_RETURN_FALSE(rc);
    if(1 == n && NULL != str_ptr) {
        str = str_ptr;
        SAFE_FREE(str_ptr);
    } else {
        for(int32_t i = 0; i < n; ++i) {
            SAFE_FREE(str_ptr);
        }
        ORTE_ERROR_LOG(ORCM_ERR_UNPACK_FAILURE);
        rv = false;
    }
    SAFE_FREE(str_ptr);
    return rv;
}

bool errcounts_impl::unpack_int32(opal_buffer_t* buffer, int32_t& value) const
{
    int32_t n = 1;
    int rc = opal_dss.unpack(buffer, &value, &n, OPAL_INT32);
    ON_FAILURE_RETURN_FALSE(rc);
    return true;
}

bool errcounts_impl::unpack_timestamp(opal_buffer_t* buffer, struct timeval& timestamp) const
{
    int32_t n = 1;
    int rc = opal_dss.unpack(buffer, &timestamp, &n, OPAL_TIMEVAL);
    ON_FAILURE_RETURN_FALSE(rc);
    return true;
}

bool errcounts_impl::unpack_data_sample(opal_buffer_t* buffer)
{
    int32_t count;
    ON_FALSE_RETURN_BOOL(unpack_int32(buffer, count));
    for(int32_t i = 0; i < count; ++i) {
        string label;
        int32_t errors;
        ON_FALSE_RETURN_BOOL(unpack_string(buffer, label));
        ON_FALSE_RETURN_BOOL(unpack_int32(buffer, errors));

        log_samples_labels_.push_back(label);
        log_samples_values_.push_back(errors);
    }
    return true;
}

bool errcounts_impl::unpack_inv_sample(opal_buffer_t* buffer)
{
    int32_t count;
    inv_samples_.clear();
    ON_FALSE_RETURN_BOOL(unpack_int32(buffer, count));
    for(int32_t i = 0; i < count; ++i) {
        string label;
        string name;
        ON_FALSE_RETURN_BOOL(unpack_string(buffer, label));
        ON_FALSE_RETURN_BOOL(unpack_string(buffer, name));
        inv_log_samples_[label] = name;
    }
    return true;
}

void errcounts_impl::generate_inventory_test_vector(opal_buffer_t* inventory_snapshot)
{
    inv_samples_.clear();

    inventory_callback("sensor_errcounts_1", "CPU_SrcID#0_Sum_DIMM#0_CE");
    inventory_callback("sensor_errcounts_2", "CPU_SrcID#0_Sum_DIMM#0_UE");
    inventory_callback("sensor_errcounts_3", "CPU_SrcID#1_Sum_DIMM#0_CE");
    inventory_callback("sensor_errcounts_4", "CPU_SrcID#1_Sum_DIMM#0_UE");

    inventory_callback("sensor_errcounts_5", "CPU_SrcID#0_CH#0_DIMM#0_CE");
    inventory_callback("sensor_errcounts_6", "CPU_SrcID#0_CH#0_DIMM#1_CE");
    inventory_callback("sensor_errcounts_7", "CPU_SrcID#0_CH#0_DIMM#2_CE");
    inventory_callback("sensor_errcounts_8", "CPU_SrcID#0_CH#0_DIMM#3_CE");

    inventory_callback("sensor_errcounts_9", "CPU_SrcID#1_CH#0_DIMM#0_CE");
    inventory_callback("sensor_errcounts_10", "CPU_SrcID#1_CH#0_DIMM#1_CE");
    inventory_callback("sensor_errcounts_11", "CPU_SrcID#1_CH#0_DIMM#2_CE");
    inventory_callback("sensor_errcounts_12", "CPU_SrcID#1_CH#0_DIMM#3_CE");

    while(true) {
        ON_FALSE_BREAK(pack_string(inventory_snapshot, plugin_name_));
        ON_FALSE_BREAK(pack_string(inventory_snapshot, hostname_));
        pack_inv_sample(inventory_snapshot);
        break;
    }
}

void errcounts_impl::generate_test_samples(bool perthread)
{
    data_samples_labels_.clear();
    data_samples_values_.clear();

    data_callback("CPU_SrcID#0_Sum_DIMM#0_CE", 0);
    data_callback("CPU_SrcID#0_Sum_DIMM#0_UE", 0);
    data_callback("CPU_SrcID#1_Sum_DIMM#0_CE", 0);
    data_callback("CPU_SrcID#1_Sum_DIMM#0_UE", 0);

    data_callback("CPU_SrcID#0_CH#0_DIMM#0_CE", 0);
    data_callback("CPU_SrcID#0_CH#0_DIMM#1_CE", 0);
    data_callback("CPU_SrcID#0_CH#0_DIMM#2_CE", 0);
    data_callback("CPU_SrcID#0_CH#0_DIMM#3_CE", 0);

    data_callback("CPU_SrcID#1_CH#0_DIMM#0_CE", 0);
    data_callback("CPU_SrcID#1_CH#0_DIMM#1_CE", 0);
    data_callback("CPU_SrcID#1_CH#0_DIMM#2_CE", 0);
    data_callback("CPU_SrcID#1_CH#0_DIMM#3_CE", 0);

    opal_buffer_t buffer;
    OBJ_CONSTRUCT(&buffer, opal_buffer_t);
    while(data_samples_values_.size() > 0) {
        ON_FALSE_BREAK(pack_string(&buffer, plugin_name_));
        ON_FALSE_BREAK(pack_string(&buffer, hostname_));
        ON_FALSE_BREAK(pack_timestamp(&buffer));
        ON_FALSE_BREAK(pack_data_sample(&buffer));

        opal_buffer_t* bptr = &buffer;
        int rc = opal_dss.pack(&errcounts_sampler_->bucket, &bptr, 1, OPAL_BUFFER);
        ON_FAILURE_BREAK(rc);
        break;
    }
    OBJ_DESTRUCT(&buffer);
}
