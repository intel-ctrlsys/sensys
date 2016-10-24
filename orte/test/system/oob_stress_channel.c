/*
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orte_config.h"

#include <stdio.h>
#include <signal.h>
#include <math.h>

#include "opal/runtime/opal_progress.h"

#include "orte/util/proc_info.h"
#include "orte/util/name_fns.h"
#include "orte/runtime/orte_globals.h"
#include "orte/mca/rml/rml.h"
#include "orte/mca/errmgr/errmgr.h"

#include "orte/runtime/runtime.h"
#include "orte/runtime/orte_wait.h"
#include "orte/util/attr.h"

#define MY_TAG 12345
#define MAX_COUNT 3

static volatile bool msgs_recvd;
static volatile bool channel_inactive = false;
static volatile bool channel_active = false;
static volatile bool msg_active = false;
static volatile orte_rml_channel_num_t channel;
static volatile int num_msgs_recvd = 0;
static volatile int num_msgs_sent = 0;

static void close_channel_callback(int status,
                                  orte_rml_channel_num_t channel_num,
                                  orte_process_name_t * peer,
                                  void * cbdata)
{
    if (ORTE_SUCCESS != status)
        opal_output(0, "close channel not successful status =%d", status);
    else
        opal_output(0, "close channel successful - channel num = %d", channel_num);
    channel_active = false;
}

static void open_channel_callback(int status,
                                  orte_rml_channel_num_t channel_num,
                                  orte_process_name_t * peer,
                                  void * cbdata)
{
    if (ORTE_SUCCESS != status) {
        opal_output(0, "open channel not successful status =%d", status);

    } else {
        channel = channel_num;
        opal_output(0, "Open channel successful - channel num = %d", channel_num);

    }
    channel_inactive = false;
}

static void send_callback(int status, orte_process_name_t *peer,
                          opal_buffer_t* buffer, orte_rml_tag_t tag,
                          void* cbdata)

{
    OBJ_RELEASE(buffer);
    num_msgs_sent++;
    if (ORTE_SUCCESS != status) {
        opal_output(0, "rml_send_nb  not successful status =%d", status);
    }
    if(num_msgs_sent == 5)
        msg_active = false;
}

static void recv_callback(int status, orte_process_name_t *sender,
                          opal_buffer_t* buffer, orte_rml_tag_t tag,
                          void* cbdata)

{
    //orte_rml_recv_cb_t *blob = (orte_rml_recv_cb_t*)cbdata;
    num_msgs_recvd++;
    opal_output(0, "recv_callback received msg =%d", num_msgs_recvd);
    if ( num_msgs_recvd == 5) {
        num_msgs_recvd =0;
        msgs_recvd = false;

    }

}

static void channel_send_callback (int status, orte_rml_channel_num_t channel,
                                   opal_buffer_t * buffer, orte_rml_tag_t tag,
                                   void *cbdata)
{
    OBJ_RELEASE(buffer);
    if (ORTE_SUCCESS != status) {
        opal_output(0, "send_nb_channel not successful status =%d", status);
    }
    msg_active = false;
}


int main(int argc, char *argv[]){
    int count;
    int msgsize;
    int *i, j, rc, n;
    orte_process_name_t peer;
    double maxpower;
    opal_buffer_t *buf;
    orte_rml_recv_cb_t blob;
    int  window;
    uint32_t timeout = 1;
    bool retry = false;
    uint8_t *msg;
    /*
     * Init
     */
    orte_init(&argc, &argv, ORTE_PROC_NON_MPI);

    if (argc > 1) {
        count = atoi(argv[1]);
        if (count < 0) {
            count = INT_MAX-1;
        }
    } else {
        count = MAX_COUNT;
    }

    peer.jobid = ORTE_PROC_MY_NAME->jobid;
    peer.vpid = ORTE_PROC_MY_NAME->vpid + 1;
    if (peer.vpid == orte_process_info.num_procs) {
        peer.vpid = 0;
    }
    window = 5;
    count =3;
    for (j = 0; j< count; j++)
    {
        if (ORTE_PROC_MY_NAME->vpid == 0)
        {
            /* rank0 starts ring */
            msg_active = true;
            for (n = 0; n< window; n++ )
            {
                buf = OBJ_NEW(opal_buffer_t);
                maxpower = (double)(j%7);
                msgsize = (int)pow(10.0, maxpower);
                opal_output(0, "Ring %d message %d size %d bytes", j,n, msgsize);
                msg = (uint8_t*)malloc(msgsize);
                opal_dss.pack(buf, msg, msgsize, OPAL_BYTE);
                free(msg);
                orte_rml.send_buffer_channel_nb(channel, buf, MY_TAG, channel_send_callback, NULL);
                OBJ_CONSTRUCT(&blob, orte_rml_recv_cb_t);
                blob.active = true;
                orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, MY_TAG,
                                        ORTE_RML_NON_PERSISTENT,
                                        orte_rml_recv_callback, &blob);
                ORTE_WAIT_FOR_COMPLETION(blob.active);
                OBJ_DESTRUCT(&blob);
                //orte_rml.send_buffer_nb(&peer, buf,MY_TAG, send_callback, NULL)
            }
            ORTE_WAIT_FOR_COMPLETION(msg_active);
            opal_output(0, "%s Ring %d completed", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), j);
            //sleep(2);
        }
        else
        {
            msg_active = true;
            for (n =0; n < window; n++) {
                OBJ_CONSTRUCT(&blob, orte_rml_recv_cb_t);
                blob.active = true;
                orte_rml.recv_buffer_nb(ORTE_NAME_WILDCARD, MY_TAG,
                                ORTE_RML_NON_PERSISTENT,
                                orte_rml_recv_callback, &blob);
                ORTE_WAIT_FOR_COMPLETION(blob.active);
                opal_output(0, "%s received message %d from %s", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), j,
                                ORTE_NAME_PRINT(&blob.name));
                /* send it along */
               buf = OBJ_NEW(opal_buffer_t);
               opal_dss.copy_payload(buf, &blob.data);
               OBJ_DESTRUCT(&blob);
               orte_rml.send_buffer_channel_nb(channel, buf, MY_TAG, channel_send_callback, NULL);
            }
            ORTE_WAIT_FOR_COMPLETION(msg_active);
            opal_output(0, "%s Ring %d completed", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME), j);
            //sleep (2);
        }
    }
    channel_active = true;
    orte_rml.close_channel ( channel,close_channel_callback, NULL);
    opal_output(0, "%s process sent close channel request waiting for completion \n",
                ORTE_NAME_PRINT(ORTE_PROC_MY_NAME));
    ORTE_WAIT_FOR_COMPLETION(channel_active);
    opal_output(0, "%s close channel complete to %s", ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                ORTE_NAME_PRINT(&peer));
    orte_finalize();
    return 0;
}
