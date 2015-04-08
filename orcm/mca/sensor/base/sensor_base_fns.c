/*
 * Copyright (c) 2010      Cisco Systems, Inc.  All rights reserved. 
 * Copyright (c) 2012      Los Alamos National Security, Inc. All rights reserved.
 * Copyright (c) 2014-2015 Intel, Inc. All rights reserved.
 *
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */


#include "orcm_config.h"
#include "orcm/constants.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/mca/rml/rml.h"

#include "opal/dss/dss.h"
#include "opal/mca/event/event.h"
#include "opal/runtime/opal_progress_threads.h"

#include "orcm/mca/db/db.h"
#include "orcm/runtime/orcm_progress.h"
#include "orcm/runtime/orcm_globals.h"
#include "orcm/mca/sensor/base/base.h"
#include "orcm/mca/sensor/base/sensor_private.h"

static bool recv_issued=false;
static bool mods_active = false;
static void take_sample(int fd, short args, void *cbdata);

/* This function will eventually be called as part of an even loop when
 * dynamic inventory collection is requested */
static void collect_inventory_info(opal_buffer_t* inventory_snapshot);

void static recv_inventory(int status, orte_process_name_t* sender,
                       opal_buffer_t *buffer,
                       orte_rml_tag_t tag, void *cbdata);

static void orcm_sensor_base_recv(int status, orte_process_name_t* sender,
                                opal_buffer_t* buffer, orte_rml_tag_t tag,
                                void* cbdata);

static void db_open_cb(int handle, int status, opal_list_t *kvs, void *cbdata)
{
    if (0 == status) {
        orcm_sensor_base.dbhandle = handle;
        orcm_sensor_base.dbhandle_acquired = true;
        /* Since we got hold of db we can setup to receive inventory
         * data from compute & io Nodes */
        if (ORTE_PROC_IS_HNP || ORTE_PROC_IS_AGGREGATOR) {
            orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD,
                                    ORCM_RML_TAG_INVENTORY,
                                    ORTE_RML_PERSISTENT,
                                    recv_inventory, NULL);
        }
    }
}

void orcm_sensor_base_start(orte_jobid_t job)
{
    orcm_sensor_active_module_t *i_module;
    int i;
    opal_buffer_t *inventory_snapshot;

    orcm_sensor_sampler_t *sampler;
    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: sensor start called",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* if no modules are active, then there is nothing to do */
    if (0 == orcm_sensor_base.modules.size) {
        return;
    }

    if (false == recv_issued) {
        OPAL_OUTPUT_VERBOSE((5, orcm_sensor_base_framework.framework_output,
                             "%s sensor:base:receive start comm",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

        orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SENSOR,
                                ORTE_RML_PERSISTENT, orcm_sensor_base_recv, NULL);
        recv_issued = true;
    }

    if (!mods_active) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                            "%s sensor:base: starting sensors",
                            ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

        if (!orcm_sensor_base.dbhandle_acquired && ORCM_PROC_IS_AGGREGATOR) {
            orcm_db.open("sensor", NULL, db_open_cb, NULL);
        }

        /* create the event base and start the progress engine, if necessary */
        if (!orcm_sensor_base.ev_active) {
            orcm_sensor_base.ev_active = true;
            if (NULL == (orcm_sensor_base.ev_base = opal_start_progress_thread("sensor", true))) {
                orcm_sensor_base.ev_active = false;
                return;
            }
        }

        /* call the start function of all modules in priority order */
        for (i=0; i < orcm_sensor_base.modules.size; i++) {
            if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
                continue;
            }
            mods_active = true;
            if (NULL != i_module->module->start) {
                i_module->module->start(job);
            }
        }

        if (mods_active && 0 < orcm_sensor_base.sample_rate && orcm_sensor_base.collect_metrics) {
            /* startup a timer to wake us up periodically
             * for a data sample, and pass in the sampler
             */
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "%s sensor:base: creating sampler with rate %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                orcm_sensor_base.sample_rate);
            sampler = OBJ_NEW(orcm_sensor_sampler_t);
            sampler->rate.tv_sec = orcm_sensor_base.sample_rate;
            sampler->log_data = orcm_sensor_base.log_samples;
            opal_event_evtimer_set(orcm_sensor_base.ev_base, &sampler->ev,
                                   take_sample, sampler);
            opal_event_evtimer_add(&sampler->ev, &sampler->rate);
        }
    } else if (!orcm_sensor_base.ev_active) {
        orcm_sensor_base.ev_active = true;
        opal_restart_progress_thread("sensor");
    }

    if(true == orcm_sensor_base.collect_inventory) {
        inventory_snapshot = OBJ_NEW(opal_buffer_t);

        if (false == orcm_sensor_base.set_dynamic_inventory) { /* Collect inventory details just once when orcmd starts */
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,"sensor:base - boot time inventory collection requested");
            /* The collect inventory call could be added to a new thread to avoid getting blocked */
            collect_inventory_info(inventory_snapshot);

        } else {
            /* Update inventory details when hotswap even occurs
             * @VINFIX: Need a way to monitor the syslog with hotswap events. */
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "sensor:base - DYNAMIC inventory collection enabled");
        }

    } else {
         opal_output_verbose(5, orcm_sensor_base_framework.framework_output,"sensor:base inventory collection not requested");
    }
    return;    
}

void collect_inventory_info(opal_buffer_t *inventory_snapshot)
{
    orcm_sensor_active_module_t *i_module;
    int32_t i,rc;
    orte_process_name_t *tgt;

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: Starting Inventory Collection",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    if (OPAL_SUCCESS != (rc = opal_dss.pack(inventory_snapshot, &orte_process_info.nodename, 1, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    /* call the inventory collection function of all enabled modules in priority order */
    for (i=0; i < orcm_sensor_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
            continue;
        }

        if (NULL != i_module->module->inventory_collect) {
            i_module->module->inventory_collect(inventory_snapshot);
        }
    }

    if (ORTE_PROC_IS_CM) {
        /* we send to our daemon */
        tgt = ORTE_PROC_MY_DAEMON;
    } else {
        tgt = ORTE_PROC_MY_HNP;
    }

    /* send Inventory data */
    if (ORCM_SUCCESS != (rc = orte_rml.send_buffer_nb(tgt, inventory_snapshot,
                                                      ORCM_RML_TAG_INVENTORY,
                                                      orte_rml_send_callback, NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(inventory_snapshot);
    }

}

static void recv_inventory(int status, orte_process_name_t* sender,
                       opal_buffer_t *buffer,
                       orte_rml_tag_t tag, void *cbdata)
{
    char *temp, *hostname;
    int32_t i, n, rc;
    orcm_sensor_active_module_t *i_module;

    /* unpack the host this came from */
    n=1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &hostname, &n, OPAL_STRING))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if(true != orcm_sensor_base.dbhandle_acquired) {
        opal_output(0,"Unable to acquire DB Handle");
        ORTE_ERROR_LOG(ORCM_ERR_TIMEOUT);
        return;
    }
    n=1;
    while (OPAL_SUCCESS == (rc = opal_dss.unpack(buffer, &temp, &n, OPAL_STRING))) {
        if (NULL != temp) {
            /* Iterate through all available components and pass the buffer to appropriate one*/
            /* find the specified module  */
            for (i=0; i < orcm_sensor_base.modules.size; i++) {
                if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
                    continue;
                }
                if (0 == strcmp(temp, i_module->component->base_version.mca_component_name)) {
                    if (NULL != i_module->module->inventory_log) {
                        i_module->module->inventory_log(hostname,buffer);
                    }
                }
            }
            free(temp);
            n=1;            
        }
    }
    if (OPAL_ERR_UNPACK_READ_PAST_END_OF_BUFFER != rc) {
        ORTE_ERROR_LOG(rc);
    }
    free(hostname);
}

void orcm_sensor_base_stop(orte_jobid_t job)
{
    orcm_sensor_active_module_t *i_module;
    int i;

    if (true == recv_issued) {
        OPAL_OUTPUT_VERBOSE((5, orcm_sensor_base_framework.framework_output,
                             "%s sensor:base:receive stop comm",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

       orte_rml.recv_cancel(ORTE_NAME_WILDCARD, ORCM_RML_TAG_SENSOR);
       recv_issued = false;
    }

    if (!mods_active) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output, "sensor stop: no active mods");
        return;
    }

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: stopping sensors",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    if (orcm_sensor_base.ev_active) {
        orcm_sensor_base.ev_active = false;
        /* stop the thread without releasing the event base */
        opal_stop_progress_thread("sensor", false);
    }

    /* call the stop function of all modules in priority order */
    for (i=0; i < orcm_sensor_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
            continue;
        }
        if (NULL != i_module->module->stop) {
            i_module->module->stop(job);
        }
    }
    /* Close the DB handle */
    if(true == orcm_sensor_base.dbhandle_acquired && ORCM_PROC_IS_AGGREGATOR) {
        orcm_db.close(orcm_sensor_base.dbhandle, NULL, NULL);
        orcm_sensor_base.dbhandle_acquired = false;
    }
    return;
}

static void take_sample(int fd, short args, void *cbdata)
{
    orcm_sensor_active_module_t *i_module;
    orcm_sensor_sampler_t *sampler = (orcm_sensor_sampler_t*)cbdata;
    int i, rc;
    
    if (!mods_active) {
        opal_output_verbose(5, orcm_sensor_base_framework.framework_output, "sensor sample: no active mods");
        return;
    }

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: sampling sensors",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* if anything is in the base cache, add it here - this
     * is safe to do since we are in the base event thread,
     * and the cache can only be accessed from there */
    if (0 < orcm_sensor_base.cache.bytes_used) {
        opal_buffer_t *bptr;
        bptr = &orcm_sensor_base.cache;
        opal_dss.copy_payload(&sampler->bucket, bptr);
        /* clear the cache */
        OBJ_DESTRUCT(&orcm_sensor_base.cache);
        OBJ_CONSTRUCT(&orcm_sensor_base.cache, opal_buffer_t);
    }

    /* call the sample function of all modules in priority order from
     * highest to lowest - the heartbeat should always be the lowest
     * priority, so it will send any collected data
     */
    for (i=0; i < orcm_sensor_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
            continue;
        }
        /* see if this sensor is included in the request */
        if (NULL != sampler->sensors &&
            NULL == strcasestr(sampler->sensors, i_module->component->base_version.mca_component_name)) {
            continue;
        }
        if (NULL != i_module->module->sample) {
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "%s sensor:base: sampling component %s",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                i_module->component->base_version.mca_component_name);
            i_module->module->sample(sampler);
        }
    }

 cleanup:
    /* execute the callback, if given */
    if (NULL != sampler->cbfunc) {
        sampler->cbfunc(&sampler->bucket, sampler->cbdata);
        OBJ_DESTRUCT(&sampler->bucket);
        OBJ_CONSTRUCT(&sampler->bucket, opal_buffer_t);
    }

    /* restart the timer, if given */
    if (0 < sampler->rate.tv_sec) {
        if (orcm_sensor_base.sample_rate && 
            sampler->rate.tv_sec != orcm_sensor_base.sample_rate) {
            sampler->rate.tv_sec = orcm_sensor_base.sample_rate;
        }
        opal_event_evtimer_add(&sampler->ev, &sampler->rate);
    } else {
        OBJ_RELEASE(sampler);
    }
    return;
}

void orcm_sensor_base_log(char *comp, opal_buffer_t *data)
{
    int i;
    orcm_sensor_active_module_t *i_module;

    /* if no modules are available, then there is nothing to do */
    if (0 == orcm_sensor_base.modules.size) {
        return;
    }

    if (NULL == comp || orcm_sensor_base.dbhandle < 0) {
        /* nothing we can do */
        return;
    }

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: logging sensor %s",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), comp);

    /* find the specified module  */
    for (i=0; i < orcm_sensor_base.modules.size; i++) {
        if (NULL == (i_module = (orcm_sensor_active_module_t*)opal_pointer_array_get_item(&orcm_sensor_base.modules, i))) {
            continue;
        }
        if (0 == strcmp(comp, i_module->component->base_version.mca_component_name)) {
            if (NULL != i_module->module->log) {
                i_module->module->log(data);
            }
            return;
        }
    }
}

void orcm_sensor_base_manually_sample(char *sensors,
                                      orcm_sensor_sample_cb_fn_t cbfunc,
                                      void *cbdata)
{
    orcm_sensor_sampler_t *sampler;

    if (!mods_active) {
        return;
    }

    opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                        "%s sensor:base: sampling sensors",
                        ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));

    /* push an event into the system to sample the sensors, collecting
     * the output in the provided buffer */
    sampler = OBJ_NEW(orcm_sensor_sampler_t);
    sampler->sensors = strdup(sensors);
    sampler->cbfunc = cbfunc;
    sampler->cbdata = cbdata;

    opal_event_set(orcm_sensor_base.ev_base, &sampler->ev, -1, OPAL_EV_WRITE, take_sample, sampler);
    opal_event_set_priority(&sampler->ev, ORTE_SYS_PRI);
    opal_event_active(&sampler->ev, OPAL_EV_WRITE, 1);
    return;
}

/* process incoming messages in order of receipt */
static void orcm_sensor_base_recv(int status, orte_process_name_t *sender,
                                opal_buffer_t *buffer, orte_rml_tag_t tag,
                                void *cbdata)
{
    orcm_sensor_cmd_flag_t command, sub_command;
    opal_buffer_t *ans;
    int sample_rate = 0;
    int rc, response, cnt, result;
    char *sensor_name;
    char *action;
    float threshold;
    bool  hi_thres;
    int   max_count, time_window;
    orte_notifier_severity_t sev;
    orcm_sensor_policy_t *plc, *newplc;
    bool found_me;

    OPAL_OUTPUT_VERBOSE((5, orcm_sensor_base_framework.framework_output,
                         "%s sensor:base:receive processing msg",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME)));

    ans = OBJ_NEW(opal_buffer_t);
    /* unpack the command */
    cnt = 1;
    if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &command,
                                              &cnt, ORCM_SENSOR_CMD_T))) {
        ORTE_ERROR_LOG(rc);
        return;
    }

    if (ORCM_SENSOR_SAMPLE_RATE_COMMAND == command) {
        /* unpack the sample rate */
        cnt = 1;
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sample_rate,
                                                  &cnt, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            return;
        }

        if (sample_rate && sample_rate != orcm_sensor_base.sample_rate) {
            /* Reset the sample rate */
            orcm_sensor_base.sample_rate = sample_rate;
            opal_output_verbose(5, orcm_sensor_base_framework.framework_output,
                                "%s sensor:base: reset sampler with rate %d",
                                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                                orcm_sensor_base.sample_rate);
        }
        /* send back the immediate success*/
        response = ORCM_SUCCESS;
        if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &response, 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            OBJ_RELEASE(ans);
            return;
        }
        if (ORTE_SUCCESS !=
            (rc = orte_rml.send_buffer_nb(sender, ans,
                                          ORCM_RML_TAG_SENSOR,
                                          orte_rml_send_callback, NULL))) {
             ORTE_ERROR_LOG(rc);
             OBJ_RELEASE(ans);
             return;
        }
    } else if (ORCM_SET_SENSOR_COMMAND == command) {
        cnt = 1;

        /* unpack the subcommand */
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sub_command,
                                                  &cnt, ORCM_SENSOR_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
            return;
        }

        switch(sub_command) {
        case ORCM_SET_SENSOR_POLICY_COMMAND:
            /* unpack sensor name */
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sensor_name,
                                                  &cnt, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }

            /* unpack threshold value */
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &threshold,
                                                  &cnt, OPAL_FLOAT))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }

            /* unpack threshold type */
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &hi_thres,
                                                  &cnt, OPAL_BOOL))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }

            /* unpack max count */
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &max_count,
                                                  &cnt, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }

            /* unpack time window */
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &time_window,
                                                  &cnt, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }

            /* unpack severity level */
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sev,
                                                  &cnt, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }

            /* unpack notification action */
            if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &action,
                                                  &cnt, OPAL_STRING))) {
                ORTE_ERROR_LOG(rc);
                goto answer;
            }

            /* look for sensor event policy; update with new setting or create new policy if not existing */
            found_me = false;
            OPAL_LIST_FOREACH(plc, &orcm_sensor_base.policy, orcm_sensor_policy_t) {
                if ( (0 == strcmp(sensor_name, plc->sensor_name)) &&
                     (hi_thres == plc->hi_thres ) &&
                     (sev == plc->severity) ) {
                    found_me = true;
                    /* update existing policy */
                    plc->threshold = threshold;
                    plc->max_count = max_count;
                    plc->time_window = time_window;
                    plc->action = strdup(action);
                    break;
                }
            }

            if ( !found_me ) {
                /* matched policy not found, insert into policy list */
                newplc = OBJ_NEW(orcm_sensor_policy_t);
                newplc->sensor_name = strdup(sensor_name);
                newplc->threshold = threshold;
                newplc->hi_thres  = hi_thres;
                newplc->max_count = max_count;
                newplc->time_window = time_window;
                newplc->severity  = sev;
                newplc->action = strdup(action);

                opal_list_append(&orcm_sensor_base.policy, &newplc->super);
                opal_output(0, "Add policy: %s %.2f %s %d %d %d %s!",
                                    newplc->sensor_name, newplc->threshold, newplc->hi_thres ? "higher" : "lower",
                                    newplc->max_count, newplc->time_window, newplc->severity, newplc->action);
            }

            /* send confirmation back to sender */
            result = 0;
            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &result, 1, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }

            break;
        default:
            rc = ORTE_ERR_BAD_PARAM;
            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &rc, 1, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }

        }


    } else if (ORCM_GET_SENSOR_COMMAND == command) {
        cnt = 1;

        /* unpack the subcommand */
        if (OPAL_SUCCESS != (rc = opal_dss.unpack(buffer, &sub_command,
                                                  &cnt, ORCM_SENSOR_CMD_T))) {
            ORTE_ERROR_LOG(rc);
            goto answer;
            return;
        }

        switch(sub_command) {
        case ORCM_GET_SENSOR_POLICY_COMMAND:

            /* pack the number of policies we have */
            cnt = opal_list_get_size(&orcm_sensor_base.policy);
            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &cnt, 1, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }

            /* for each queue, */
            OPAL_LIST_FOREACH(plc, &orcm_sensor_base.policy, orcm_sensor_policy_t) {
                /* pack sensor name */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &plc->sensor_name,
                                                  1, OPAL_STRING))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }

                /* pack threshold value */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &plc->threshold,
                                                  1, OPAL_FLOAT))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }

                /* pack threshold type */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &plc->hi_thres,
                                                  1, OPAL_BOOL))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }

                /* pack max count */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &plc->max_count,
                                                  1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }

                /* pack time window */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &plc->time_window,
                                                  1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }

                /* pack severity level */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &plc->severity,
                                                  1, OPAL_INT))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }

                /* pack notification action */
                if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &plc->action,
                                                  1, OPAL_STRING))) {
                    ORTE_ERROR_LOG(rc);
                    goto answer;
                }
            }

            break;
        default:
            rc = ORTE_ERR_BAD_PARAM;
            if (OPAL_SUCCESS != (rc = opal_dss.pack(ans, &rc, 1, OPAL_INT))) {
                ORTE_ERROR_LOG(rc);
                OBJ_RELEASE(ans);
                return;
            }

        }

    }

answer:

    if (ORTE_SUCCESS != (rc = orte_rml.send_buffer_nb(sender, ans,
                                                      ORCM_RML_TAG_SENSOR,
                                                      orte_rml_send_callback,
                                                      NULL))) {
        ORTE_ERROR_LOG(rc);
        OBJ_RELEASE(ans);
        return;
    }

}

void orcm_sensor_base_collect(int fd, short args, void *cbdata)
{
    orcm_sensor_xfer_t *x = (orcm_sensor_xfer_t*)cbdata;

    /* now that we are in the correct thread,
     * copy the data to the base cache bucket
     * so it can be swept up by the next update */
    opal_dss.copy_payload(&orcm_sensor_base.cache, &x->bucket);
    /* release memory */
    OBJ_RELEASE(x);
}
