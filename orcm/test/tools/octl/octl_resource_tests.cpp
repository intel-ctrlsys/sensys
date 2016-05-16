/*
 * Copyright (c) 2016  Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include <iostream>
#include "octl_resource_tests.h"

using namespace std;

#define MY_ORCM_SUCCESS 0

void ut_octl_resource_tests::RecvBufferTimeOut(orte_process_name_t* peer,
                                               orte_rml_tag_t tag,
                                               bool persistent,
                                               orte_rml_buffer_callback_fn_t cbfunc,
                                               void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    xfer->name.jobid = 0;
    xfer->name.vpid = 0;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    int rv = ORTE_SUCCESS;
    int count = 0;
    opal_dss.pack(&xfer->data, &rv, 1, OPAL_INT);
    opal_dss.pack(&xfer->data, &count, 1, OPAL_INT);
    xfer->active = true;
}

void ut_octl_resource_tests::RecvBufferNodeErr(orte_process_name_t* peer,
                                               orte_rml_tag_t tag,
                                               bool persistent,
                                               orte_rml_buffer_callback_fn_t cbfunc,
                                               void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    xfer->name.jobid = 0;
    xfer->name.vpid = 0;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    const char* name = "status";
    opal_dss.pack(&xfer->data, &name, 1, OPAL_STRING);
    xfer->active = false;
}

void ut_octl_resource_tests::RecvBufferOneNode(orte_process_name_t* peer,
                                               orte_rml_tag_t tag,
                                               bool persistent,
                                               orte_rml_buffer_callback_fn_t cbfunc,
                                               void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    xfer->name.jobid = 0;
    xfer->name.vpid = 0;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    int num_nodes = 1;
    opal_dss.pack(&xfer->data, &num_nodes, 1, OPAL_INT);
    xfer->active = false;
}

void ut_octl_resource_tests::RecvBufferInstNode(orte_process_name_t* peer,
                                               orte_rml_tag_t tag,
                                               bool persistent,
                                               orte_rml_buffer_callback_fn_t cbfunc,
                                               void* cbdata)
{
    orte_rml_recv_cb_t* xfer = (orte_rml_recv_cb_t*)cbdata;
    xfer->name.jobid = 0;
    xfer->name.vpid = 0;
    OBJ_DESTRUCT(&xfer->data);
    OBJ_CONSTRUCT(&xfer->data, opal_buffer_t);
    int num_nodes = 1;
    opal_dss.pack(&xfer->data, &num_nodes, 1, OPAL_INT);
    orcm_node_t *node = OBJ_NEW(orcm_node_t);
    const char* name = "status";
    node->name = strdup(name);
    node->state=ORCM_NODE_STATE_UNKNOWN;
    node->rack = NULL;
    opal_dss.pack(&xfer->data, node, 1, ORCM_NODE);
    xfer->active = false;
}

int ut_octl_resource_tests::UnpackBufferNode(opal_buffer_t *buffer, void *dest,
                                             int32_t *max_num_values,
                                             opal_data_type_t type)
{
    if (OPAL_INT == type) {
        *((int*)dest) = 1;
    }
    if (ORCM_NODE == type) {
        orcm_node_t **nodes;
        nodes = (orcm_node_t**) dest;
        nodes[0]= OBJ_NEW(orcm_node_t);
        nodes[0]->name = strdup("name");
        nodes[0]->state = ORTE_NODE_STATE_UNKNOWN;
    }
    return ORCM_SUCCESS;
}


int ut_octl_resource_tests::PackBufferCmd(opal_buffer_t *buffer, const void *src,
                                             int32_t num_values,
                                             opal_data_type_t type)
{
    if (type == ORCM_SCD_CMD_T) {
        return ORCM_ERROR;
    }

    return ORCM_SUCCESS;

}



void ut_octl_resource_tests::SetUpTestCase()
{
    ut_octl_tests::SetUpTestCase();

}

void ut_octl_resource_tests::TearDownTestCase()
{
    ut_octl_tests::TearDownTestCase();

}

TEST_F(ut_octl_resource_tests, resource_status_send_negative)
{
    const char* cmdlist[3] = {
        "resource",
        "status",
        NULL
    };

    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_resource_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_resource_tests, resource_status_recv_timeout)
{
    const char* cmdlist[3] = {
        "resource",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = RecvBufferTimeOut;
    int rv = orcm_octl_resource_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb =  saved_recv_buffer;
}

TEST_F(ut_octl_resource_tests, resource_status_num_node_unpack_error)
{
    const char* cmdlist[3] = {
        "resource",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = RecvBufferNodeErr;
    int rv = orcm_octl_resource_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb =  saved_recv_buffer;
}

TEST_F(ut_octl_resource_tests, resource_status_node_unpack_error)
{
    const char* cmdlist[3] = {
        "resource",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = RecvBufferOneNode;
    int rv = orcm_octl_resource_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb =  saved_recv_buffer;
}

TEST_F(ut_octl_resource_tests, resource_status_inst_node)
{
    const char* cmdlist[3] = {
        "resource",
        "status",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    opal_dss_unpack_fn_t saved_unpack_buffer;

    saved_unpack_buffer = opal_dss.unpack;

    orte_rml.recv_buffer_nb = RecvBufferInstNode;
    opal_dss.unpack = UnpackBufferNode;

    int rv = orcm_octl_resource_status((char**)cmdlist);
    ASSERT_EQ(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb =  saved_recv_buffer;
    opal_dss.unpack = saved_unpack_buffer;
}

TEST_F(ut_octl_resource_tests, resource_status_cmd_pack_error)
{
    const char* cmdlist[3] = {
        "resource",
        "status",
        NULL
    };

    opal_dss_pack_fn_t saved_pack_fn;

    saved_pack_fn = opal_dss.pack;

    opal_dss.pack = PackBufferCmd;
    int rv = orcm_octl_resource_status((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
    opal_dss.pack = saved_pack_fn;
}

TEST_F(ut_octl_resource_tests, resource_drain_arg_negative)
{
    const char* cmdlist[3] = {
        "resource",
        "drain",
        NULL
    };

    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_resource_drain((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_resource_tests, resource_drain_send_negative)
{
    const char* cmdlist[4] = {
        "resource",
        "drain",
        "node",
        NULL
    };

    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_resource_drain((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_resource_tests, resource_drain_recv_timeout)
{
    const char* cmdlist[4] = {
        "resource",
        "drain",
        "node",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = RecvBufferTimeOut;
    int rv = orcm_octl_resource_drain((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb =  saved_recv_buffer;
}

TEST_F(ut_octl_resource_tests, resource_drain_result_unpack_error)
{
    const char* cmdlist[4] = {
        "resource",
        "drain",
        "node",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = RecvBufferNodeErr;
    int rv = orcm_octl_resource_drain((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb =  saved_recv_buffer;
}

TEST_F(ut_octl_resource_tests, resource_drain_cmd_pack_error)
{
    const char* cmdlist[4] = {
        "resource",
        "drain",
        "node",
        NULL
    };

    opal_dss_pack_fn_t saved_pack_fn;

    saved_pack_fn = opal_dss.pack;

    opal_dss.pack = PackBufferCmd;
    int rv = orcm_octl_resource_drain((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
    opal_dss.pack = saved_pack_fn;
}

TEST_F(ut_octl_resource_tests, resource_drain_illegal_node)
{
    const char* cmdlist[4] = {
        "resource",
        "drain",
        "node[",
        NULL
    };

    int rv = orcm_octl_resource_drain((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_resource_tests, resource_resume_arg_negative)
{
    const char* cmdlist[3] = {
        "resource",
        "resume",
        NULL
    };

    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_resource_resume((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_resource_tests, resource_resume_send_negative)
{
    const char* cmdlist[4] = {
        "resource",
        "resume",
        "node",
        NULL
    };

    next_send_result = ORTE_ERROR;
    int rv = orcm_octl_resource_resume((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}

TEST_F(ut_octl_resource_tests, resource_resume_recv_timeout)
{
    const char* cmdlist[4] = {
        "resource",
        "resume",
        "node",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = RecvBufferTimeOut;
    int rv = orcm_octl_resource_resume((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb =  saved_recv_buffer;
}

TEST_F(ut_octl_resource_tests, resource_resume_result_unpack_error)
{
    const char* cmdlist[4] = {
        "resource",
        "resume",
        "node",
        NULL
    };

    orte_rml_module_recv_buffer_nb_fn_t saved_recv_buffer;

    saved_recv_buffer = orte_rml.recv_buffer_nb;

    orte_rml.recv_buffer_nb = RecvBufferNodeErr;
    int rv = orcm_octl_resource_resume((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);

    orte_rml.recv_buffer_nb =  saved_recv_buffer;
}

TEST_F(ut_octl_resource_tests, resource_resume_cmd_pack_error)
{
    const char* cmdlist[4] = {
        "resource",
        "resume",
        "node",
        NULL
    };

    opal_dss_pack_fn_t saved_pack_fn;

    saved_pack_fn = opal_dss.pack;

    opal_dss.pack = PackBufferCmd;
    int rv = orcm_octl_resource_resume((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
    opal_dss.pack = saved_pack_fn;
}

TEST_F(ut_octl_resource_tests, resource_resume_illegal_node)
{
    const char* cmdlist[4] = {
        "resource",
        "resume",
        "node[",
        NULL
    };

    int rv = orcm_octl_resource_resume((char**)cmdlist);
    ASSERT_NE(MY_ORCM_SUCCESS, rv);
}
