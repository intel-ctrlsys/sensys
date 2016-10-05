/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 *
 * Copyright (c) 2015-2016     Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 *
 */

/*
 * includes
 */
#include "orte_config.h"

#include <string.h>

#include "orte/constants.h"
#include "orte/types.h"

#include "opal/dss/dss.h"
#include "opal/util/output.h"
#include "opal/util/timings.h"
#include "opal/class/opal_list.h"

#include "orte/mca/errmgr/errmgr.h"
#include "orte/runtime/orte_globals.h"
#include "orte/runtime/orte_wait.h"
#include "orte/util/name_fns.h"

#include "orte/mca/rml/rml.h"
#include "orte/mca/rml/base/base.h"
#include "orte/mca/rml/base/rml_contact.h"


static orte_rml_channel_t * get_channel ( orte_process_name_t * peer,
                                          bool recv);
static int send_open_channel_reply (orte_process_name_t *peer,
                                    orte_rml_channel_t *channel,
                                    bool accept);
void orte_rml_base_close_channel(int fd, short flags, void *cbdata)
{
    orte_rml_send_request_t *req = (orte_rml_send_request_t*)cbdata;
    orte_rml_close_channel_t *close_chan;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_close_channel to peer %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(&req->post.close_channel.channel->peer)));
    OPAL_TIMING_EVENT((&tm_rml, "to %s", ORTE_NAME_PRINT(&req->post.close_channel.channel->peer)));
    close_chan = OBJ_NEW(orte_rml_close_channel_t);
    close_chan->channel = req->post.close_channel.channel;
    close_chan->cbfunc = req->post.close_channel.cbfunc;
    close_chan->cbdata = req->post.close_channel.cbdata;
    OBJ_RELEASE(req);
    close_chan->status = ORTE_ERR_CHANNEL_BUSY;
    ORTE_RML_CLOSE_CHANNEL_COMPLETE(close_chan);
    OBJ_RELEASE(close_chan);
}

void orte_rml_base_send_close_channel ( orte_rml_close_channel_t *close_chan)
{
    opal_buffer_t *buffer;
    // send msg to peer to close channel.
    buffer = OBJ_NEW (opal_buffer_t);
    /* pack the channel number*/
    opal_dss.pack(buffer, &close_chan->channel->peer_channel, 1, OPAL_UINT32);
    orte_rml.send_buffer_nb( &close_chan->channel->peer, buffer, ORTE_RML_TAG_CLOSE_CHANNEL_REQ,
                             orte_rml_base_close_channel_send_callback,
                             close_chan);
}

void orte_rml_base_close_channel_send_callback ( int status,
        orte_process_name_t* sender,
        opal_buffer_t* buffer,
        orte_rml_tag_t tag,
        void* cbdata)
{
    // this is the send call back for open channel request
    orte_rml_close_channel_t *req = (orte_rml_close_channel_t*) cbdata;
    orte_process_name_t peer = req->channel->peer;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_close_channel_send_callback to peer %s status = %d",
                         ORTE_NAME_PRINT(sender),
                         ORTE_NAME_PRINT(&peer), status));
    req->status = status;
    // if the message could not be sent log error
    if  (ORTE_SUCCESS != req->status)
        ORTE_ERROR_LOG (req->status);
    //complete the req.
    ORTE_RML_CLOSE_CHANNEL_COMPLETE(req);
    opal_pointer_array_set_item ( &orte_rml_base.open_channels, req->channel->channel_num, NULL);
    // release the channel object and the req.
    OBJ_RELEASE(req->channel);
    OBJ_RELEASE(req);
    OBJ_RELEASE(buffer);
}

void orte_rml_base_open_channel(int fd, short flags, void *cbdata)
{
    orte_rml_send_request_t *req = (orte_rml_send_request_t*)cbdata;
    orte_process_name_t peer;
    orte_rml_open_channel_t *open_chan;
    orte_rml_channel_t *channel;
    peer = req->post.open_channel.dst;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel to peer %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(&peer)));
    OPAL_TIMING_EVENT((&tm_rml, "to %s", ORTE_NAME_PRINT(&peer)));
    /* return error if a channel already exists */
    if ( NULL != (channel = get_channel (&peer, false)))
    {
        req->post.open_channel.status = ORTE_ERR_OPEN_CHANNEL_DUPLICATE;
        req->post.open_channel.channel = channel;
        ORTE_RML_OPEN_CHANNEL_COMPLETE(&req->post.open_channel);
        OBJ_RELEASE(req);
        return;
    }
    channel = OBJ_NEW(orte_rml_channel_t);
    channel->channel_num = opal_pointer_array_add (&orte_rml_base.open_channels, channel);
    channel->peer = peer;
    open_chan = OBJ_NEW(orte_rml_open_channel_t);
    open_chan->dst = peer;
    open_chan->cbfunc = req->post.open_channel.cbfunc;
    open_chan->cbdata = req->post.open_channel.cbdata;
    OBJ_RELEASE(req);
     // associate open channel request and the newly created channel object
    open_chan->channel = channel;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel to peer %s ",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(&peer)));
    ORTE_RML_OPEN_CHANNEL_COMPLETE(open_chan);
    opal_pointer_array_set_item ( &orte_rml_base.open_channels, open_chan->channel->channel_num, NULL);
    OBJ_RELEASE(open_chan->channel);
    OBJ_RELEASE(open_chan);
}

void orte_rml_base_open_channel_send_callback ( int status,
        orte_process_name_t* sender,
        opal_buffer_t* buffer,
        orte_rml_tag_t tag,
        void* cbdata)
{
    // this is the send call back for open channel request
    orte_rml_open_channel_t *req = (orte_rml_open_channel_t*) cbdata;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel_send_callback to peer %s status = %d",
                         ORTE_NAME_PRINT(sender),
                         ORTE_NAME_PRINT(&req->dst), status));
    // if the message was not sent we should retry or complete the request appropriately
    if (status!= ORTE_SUCCESS)
    {
        req->status = status;
        ORTE_RML_OPEN_CHANNEL_COMPLETE(req);
        opal_pointer_array_set_item ( &orte_rml_base.open_channels, req->channel->channel_num, NULL);
        OBJ_RELEASE(req->channel);
        OBJ_RELEASE(req);
    }
    else {
        // start a timer for response from peer
    }
    OBJ_RELEASE(buffer);
}

void orte_rml_base_open_channel_resp_callback (int status,
                                          orte_process_name_t* peer,
                                          struct opal_buffer_t* buffer,
                                          orte_rml_tag_t tag,
                                          void* cbdata)
{
    orte_rml_open_channel_t *req = (orte_rml_open_channel_t*) cbdata;
    orte_rml_channel_t * channel = req->channel;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel_resp_callback to peer %s status = %d channel = %p",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(peer), status,
                         (void*)channel));
    int32_t rc;
    bool peer_resp = false;
    int32_t count = 1;
    // unpack peer  response from buffer to determine if peer has accepted the open request
    if ((ORTE_SUCCESS == (rc = opal_dss.unpack(buffer, &peer_resp, &count, OPAL_BOOL))) && peer_resp) {

        OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                             "%s rml_open_channel_resp_callback to peer response = %d",
                             ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                             peer_resp));
        /* response will contain the peer channel number -  the peer does not have the
           option to change the channel attributes
            unpack  and get peer channel number.*/
        if (ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &channel->peer_channel, &count, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            req->status = ORTE_ERR_UNPACK_FAILURE;
            opal_pointer_array_set_item ( &orte_rml_base.open_channels, req->channel->channel_num, NULL);
            OBJ_RELEASE(req->channel);
            // TBD : should we send a close channel to the peer??
        }
        else {
            req->status = ORTE_SUCCESS;
            req->channel->state = orte_rml_channel_open;
        }
    }
    else {
        if (rc) {
            ORTE_ERROR_LOG(rc);
            req->status = ORTE_ERR_UNPACK_FAILURE;
        } else {
            req->status = ORTE_ERR_OPEN_CHANNEL_PEER_REJECT;
        }
        opal_pointer_array_set_item ( &orte_rml_base.open_channels, req->channel->channel_num, NULL);
        OBJ_RELEASE(req->channel);
    }
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel_resp_callback to peer %s status = %d channel =%p num = %d",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(peer), req->status,
                         (void*)channel, channel->channel_num));
    ORTE_RML_OPEN_CHANNEL_COMPLETE(req);
    OBJ_RELEASE(req);
    return;
}

void orte_rml_open_channel_recv_callback (int status,
        orte_process_name_t* peer,
        struct opal_buffer_t* buffer,
        orte_rml_tag_t tag,
        void* cbdata)
{
    orte_rml_channel_t *channel;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_open_channel_recv_callback from peer %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(peer)));
    if (NULL == (channel = get_channel (peer, true))) {
        /* create a new channel for the req */
        channel = OBJ_NEW(orte_rml_channel_t);
        channel->channel_num = opal_pointer_array_add (&orte_rml_base.open_channels, channel);
        OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                             "rml_open_channel_recv_callback channel num =%d",
                             channel->channel_num));
        channel->peer = *peer;
        channel->recv = true;
        send_open_channel_reply (peer, NULL, false);
        opal_pointer_array_set_item ( &orte_rml_base.open_channels, channel->channel_num, NULL);
        OBJ_RELEASE(channel);
    }
    else {
        OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                             "rml_open_channel_recv_callback OOPS CHANNEL EXISTS ALREADY channel num =%d",
                             channel->channel_num));
        send_open_channel_reply (peer, channel, false);
    }
}

static int send_open_channel_reply (orte_process_name_t *peer,
                                     orte_rml_channel_t *channel,
                                     bool accept)
{
    opal_buffer_t *buffer;
    int32_t rc;
    buffer = OBJ_NEW (opal_buffer_t);
    if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &accept , 1, OPAL_BOOL))) {
        ORTE_ERROR_LOG(rc);
        return rc;
    }
    if (accept) {
        if (OPAL_SUCCESS != (rc = opal_dss.pack(buffer, &channel->channel_num , 1, OPAL_INT))) {
            ORTE_ERROR_LOG(rc);
            return rc;
        }
    }
    /* TBD: should specify reason for reject
      send open channel response to sender */
    orte_rml.send_buffer_nb ( peer, buffer, ORTE_RML_TAG_OPEN_CHANNEL_RESP,
                              orte_rml_base_open_channel_reply_send_callback,
                              channel);

    return rc;
}

static orte_rml_channel_t * get_channel ( orte_process_name_t * peer,
                                          bool recv)
{
    orte_rml_channel_t *channel = NULL;
    int32_t i = 0;
    for (i=0; i < orte_rml_base.open_channels.size; i++) {
        if (NULL != (channel = (orte_rml_channel_t*) opal_pointer_array_get_item (&orte_rml_base.open_channels, i))) {
            /* compare basic properties */
            if ((OPAL_EQUAL == orte_util_compare_name_fields(ORTE_NS_CMP_ALL, &channel->peer, peer)) &&
                                            ((orte_rml_channel_open == channel->state) ||
                                             (orte_rml_channel_opening == channel->state)) &&
                                             (channel->recv == recv))
            {
                return channel;

            }
        }
    }
    return NULL;
}

void orte_rml_base_open_channel_reply_send_callback ( int status,
        orte_process_name_t* sender,
        opal_buffer_t* buffer,
        orte_rml_tag_t tag,
        void* cbdata)
{
    // this is the send call back for open channel reply
    orte_rml_channel_t *channel = (orte_rml_channel_t*) cbdata;
    // if the message was not sent we should retry or release the channel resources
    if (status!= ORTE_SUCCESS)
    {
        ORTE_ERROR_LOG (status);
        // release channel
        if(NULL != channel) {
            opal_pointer_array_set_item ( &orte_rml_base.open_channels, channel->channel_num, NULL);
            OBJ_RELEASE(channel);
        } else {
            // we did not accept the request so nothing to do
        }
    }
    // if success then release the buffer and do open channel request completion after receiving response from peer
    OBJ_RELEASE(buffer);
}

orte_rml_channel_t * orte_rml_base_get_channel (orte_rml_channel_num_t chan_num) {
    orte_rml_channel_t * channel;

    channel = (orte_rml_channel_t*) opal_pointer_array_get_item (&orte_rml_base.open_channels, chan_num);
    if ((NULL != channel) && (orte_rml_channel_open == channel->state))
        return channel;
    else
        return NULL;
    return channel;
}

void orte_rml_base_prep_send_channel (orte_rml_channel_t *channel,
                                      orte_rml_send_t *send)
{
    // add channel number
    send->dst_channel = channel->peer_channel;
}

void orte_rml_close_channel_recv_callback (int status,
        orte_process_name_t* peer,
        struct opal_buffer_t* buffer,
        orte_rml_tag_t tag,
        void* cbdata)
{
    // find the channel and close it or log error
    orte_rml_channel_t *channel;
    int32_t count =1, rc;
    orte_rml_channel_num_t channel_num =5;
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_close_channel_recv_callback from peer %s",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         ORTE_NAME_PRINT(peer)));
    /* unpack channel number */
    if(ORTE_SUCCESS != (rc = opal_dss.unpack(buffer, &channel_num,
                                          &count, OPAL_UINT32))) {
        ORTE_ERROR_LOG(rc);
        return;
    }
    channel = orte_rml_base_get_channel(channel_num);
    OPAL_OUTPUT_VERBOSE((1, orte_rml_base_framework.framework_output,
                         "%s rml_close_channel_recv_callback for channel num =%d channel=%p",
                         ORTE_NAME_PRINT(ORTE_PROC_MY_NAME),
                         channel_num, (void*)channel));
    if (NULL != channel) {
          opal_pointer_array_set_item ( &orte_rml_base.open_channels, channel->channel_num, NULL);
          OBJ_RELEASE(channel);
    } else {
        ORTE_ERROR_LOG(OPAL_ERR_BAD_PARAM);
    }
}
