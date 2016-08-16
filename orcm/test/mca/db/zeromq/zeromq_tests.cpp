//
// Copyright (c) 2016      Intel, Inc. All rights reserved.
// $COPYRIGHT$
//
// Additional copyrights may follow
//
// $HEADER$

#include "zeromq_tests.hpp"

#include <sstream>

// ORCM
extern "C" {
    #include "orcm/runtime/orcm_globals.h"
    #include "opal/class/opal_list.h"
    #include "orte/mca/errmgr/errmgr.h"
    #include "orcm/util/utils.h"
}

extern "C" {
    extern mca_db_zeromq_module_t mca_db_zeromq_module;
    extern void orcm_db_zeromq_output_callback(int level, const char* msg);
    extern void orcm_db_zeromq_print_value(const opal_value_t *kv, std::stringstream& ss);
    extern std::string orcm_db_zeromq_print_orcm_json_format(opal_list_t *kvs, bool inventory = false);
    extern void orcm_db_zeromq_print_time_value(const struct timeval *time, std::stringstream& ss);
}

void* ZeroMQTestClass::POINTER = (void*)(uint64_t)0x0000000012345678;

std::string outputLine = "";

void MyOutput(int level, const char* message)
{
    outputLine = message;
}

TEST_F(BasicTestFixture, API_init_finalize_tests)
{
    struct orcm_db_base_module_t* mod = (struct orcm_db_base_module_t*)(&mca_db_zeromq_module);
    EXPECT_EQ(0, mod->init(mod));
    EXPECT_EQ(0, mod->init(mod));
    mod->finalize(mod);
    mod->finalize(mod);
    ZeroMQTestClass* obj = dynamic_cast<ZeroMQTestClass*>(orcm_db_zeromq_object.get());
    obj->failCtxNew_ = true;
    EXPECT_EQ(-1, mod->init(mod));
    obj->failCtxNew_ = false;
    obj->failCtxSet_ = true;
    EXPECT_EQ(-1, mod->init(mod));
    obj->failCtxSet_ = false;
    obj->failSocket_ = true;
    EXPECT_EQ(-1, mod->init(mod));
    obj->failSocket_ = false;
    obj->failSetSocketOpt_ = true;
    EXPECT_EQ(-1, mod->init(mod));
    obj->failSetSocketOpt_ = false;
}

TEST(NoFixture, output_tests)
{
    outputLine = "";
    Publisher* publisher = new ZeroMQTestClass();
    publisher->Output(0, "This is thrown away.");
    EXPECT_STREQ("", outputLine.c_str());
    publisher->SetOutputCallback(MyOutput);
    publisher->Output(0, "This is NOT thrown away.");
    EXPECT_STREQ("This is NOT thrown away.", outputLine.c_str());

    orcm_db_zeromq_output_callback(0, "Testing the ORCM callback.");
    outputLine = "";
}

TEST_F(BasicTestFixture, API_publish_tests)
{
    struct orcm_db_base_module_t* mod = (struct orcm_db_base_module_t*)(&mca_db_zeromq_module);
    ZeroMQTestClass* obj = dynamic_cast<ZeroMQTestClass*>(orcm_db_zeromq_object.get());
    EXPECT_EQ(0, mod->init(mod));
    EXPECT_EQ(0, mod->store_new(mod, ORCM_DB_ENV_DATA, NULL, NULL));
    obj->failPublish_ = true;
    EXPECT_EQ(-1, mod->store_new(mod, ORCM_DB_ENV_DATA, NULL, NULL));
    obj->failPublish_ = false;
    mod->finalize(mod);
    EXPECT_EQ(-1, mod->store_new(mod, ORCM_DB_ENV_DATA, NULL, NULL));
}

TEST_F(BasicTestFixture2, API_real_zmq_tests)
{
    struct orcm_db_base_module_t* mod = (struct orcm_db_base_module_t*)(&mca_db_zeromq_module);
    ZeroMQTestClass2* obj = dynamic_cast<ZeroMQTestClass2*>(orcm_db_zeromq_object.get());
    obj->mockAddress_ = true;
    EXPECT_EQ(0, mod->init(mod));
    EXPECT_EQ(0, mod->store_new(mod, ORCM_DB_ENV_DATA, NULL, NULL));
    mod->finalize(mod);
    obj->mockAddress_ = false;
}

TEST_F(BasicTestFixture3, API_real_zmq_failure_tests)
{
    struct orcm_db_base_module_t* mod = (struct orcm_db_base_module_t*)(&mca_db_zeromq_module);
    ZeroMQTestClass3* obj = dynamic_cast<ZeroMQTestClass3*>(orcm_db_zeromq_object.get());
    obj->mockAddress_ = true;
    EXPECT_EQ(-1, mod->init(mod));
    mod->finalize(mod);
    obj->mockAddress_ = false;
}

TEST_F(BasicTestFixture, API_more_publish_tests)
{
    struct orcm_db_base_module_t* mod = (struct orcm_db_base_module_t*)(&mca_db_zeromq_module);
    ZeroMQTestClass* obj = dynamic_cast<ZeroMQTestClass*>(orcm_db_zeromq_object.get());
    EXPECT_EQ(0, mod->init(mod));
    obj->failMsgSend_ = true;
    EXPECT_EQ(-1, mod->store_new(mod, ORCM_DB_ENV_DATA, NULL, NULL));
    obj->failMsgSend2_ = true;
    EXPECT_EQ(-1, mod->store_new(mod, ORCM_DB_ENV_DATA, NULL, NULL));
    obj->failMsgSend_ = false;
    obj->failMsgSend2_ = false;
    obj->failMsgInit_ = 1;
    EXPECT_EQ(-2, mod->store_new(mod, ORCM_DB_ENV_DATA, NULL, NULL));
    obj->failMsgInit_ = 2;
    EXPECT_EQ(-2, mod->store_new(mod, ORCM_DB_ENV_DATA, NULL, NULL));
    mod->finalize(mod);
}

TEST_F(ComponentTestFixture, component_tests)
{
    EXPECT_EQ(0, orcm_db_zeromq_component_register());
    EXPECT_TRUE(orcm_db_zeromq_component_avail());
    void* mod = (void*)orcm_db_zeromq_component_create(NULL);
    ASSERT_NOT_NULL(mod);
    free((mca_db_zeromq_module_t*)mod);
}

TEST_F(ComponentTestFixture, component_negative_tests)
{
    ZeroMQTestClass* obj = dynamic_cast<ZeroMQTestClass*>(orcm_db_zeromq_object.get());
    obj->failCtxNew_ = true;
    void* mod = (void*)orcm_db_zeromq_component_create(NULL);
    EXPECT_NULL(mod);
    obj->failCtxNew_ = false;
}

TEST_F(BasicTestFixture, Json_formatter_tests)
{

    opal_list_t* kv = NULL;
    std::stringstream ss;
    std::string str;
    kv = OBJ_NEW(opal_list_t);
    kv = nullptr;
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    const char* hostname = "test_host";
    orcm_value_t *host_name = orcm_util_load_orcm_value((char*)"hostname", (void*)hostname, OPAL_STRING, NULL);
    if (NULL != host_name)  {
        opal_list_append(kv, (opal_list_item_t*)host_name);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"hostname\",\"value\":\"test_host\"}]}", str);
    ORCM_RELEASE(kv);


    orcm_value_t* mv = NULL;
    kv = OBJ_NEW(opal_list_t);
    bool b_value = true;
    mv = orcm_util_load_orcm_value((char*)"bool", (void*)(&b_value), OPAL_BOOL, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }

    b_value = false;
    mv = orcm_util_load_orcm_value((char*)"bool", (void*)(&b_value), OPAL_BOOL, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"bool\",\"value\":true},{\"key\":\"bool\",\"value\":false}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    double d_value = 37.5;
    mv = orcm_util_load_orcm_value((char*)"double", (void*)(&d_value), OPAL_DOUBLE, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }

    float f_value = 9.9;
    mv = orcm_util_load_orcm_value((char*)"float", (void*)(&f_value), OPAL_FLOAT, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"double\",\"value\":37.5},{\"key\":\"float\",\"value\":9.9}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    int i_value = 4;
    mv = orcm_util_load_orcm_value((char*)"int", (void*)(&i_value), OPAL_INT, (char*)"");
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"int\",\"value\":4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    mv = orcm_util_load_orcm_value((char*)"int8", (void*)(&i_value), OPAL_INT8, (char*)"c");
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"int8\",\"value\":\x4,\"units\":\"c\"}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    mv = orcm_util_load_orcm_value((char*)"int16", (void*)(&i_value), OPAL_INT16, (char*)"");
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"int16\",\"value\":4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    mv = orcm_util_load_orcm_value((char*)"int32", (void*)(&i_value), OPAL_INT32, (char*)"");
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"int32\",\"value\":4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    int64_t i64_value = 4;
    mv = orcm_util_load_orcm_value((char*)"int64", (void*)(&i64_value), OPAL_INT64, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"int64\",\"value\":4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    mv = orcm_util_load_orcm_value((char*)"uint", (void*)(&i_value), OPAL_UINT, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv, true);
    EXPECT_EQ("{\"data\":[{\"key\":\"uint\",\"value\":4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    mv = orcm_util_load_orcm_value((char*)"uint8", (void*)(&i_value), OPAL_UINT8, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"uint8\",\"value\":\x4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    mv = orcm_util_load_orcm_value((char*)"uint16", (void*)(&i_value), OPAL_UINT16, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"uint16\",\"value\":4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    mv = orcm_util_load_orcm_value((char*)"uint32", (void*)(&i_value), OPAL_UINT32, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"uint32\",\"value\":4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    uint64_t ui64_value = 4;
    mv = orcm_util_load_orcm_value((char*)"uint64", (void*)(&ui64_value), OPAL_UINT64, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"uint64\",\"value\":4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    mv = orcm_util_load_orcm_value((char*)"size", (void*)(&ui64_value), OPAL_SIZE, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"size\",\"value\":4}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    int pid_value = 2981;
    mv = orcm_util_load_orcm_value((char*)"pid", (void*)(&pid_value), OPAL_PID, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_EQ("{\"data\":[{\"key\":\"pid\",\"value\":2981}]}", str);
    ORCM_RELEASE(kv);


    kv = OBJ_NEW(opal_list_t);
    struct timeval tval;
    memset(&tval, 0, sizeof(struct timeval));
    mv = orcm_util_load_orcm_value((char*)"time", (void*)(&tval), OPAL_TIMEVAL, NULL);
    if (NULL != mv) {
        opal_list_append(kv, (opal_list_item_t*)mv);
    }
    str = orcm_db_zeromq_print_orcm_json_format(kv);
    EXPECT_STRNE("{\"data\":[]}", str.c_str());
    ORCM_RELEASE(kv);

    opal_value_t ovt;
    OBJ_CONSTRUCT(&ovt, opal_value_t);
    ovt.type = 254;
    memset(&ovt.data, 0, sizeof(ovt.data));
    ss.str("");
    orcm_db_zeromq_print_value(&ovt, ss);
    EXPECT_STREQ("\"\"", ss.str().c_str());
    OBJ_DESTRUCT(&ovt);
}
