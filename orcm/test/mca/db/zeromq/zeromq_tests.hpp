//
// Copyright (c) 2016      Intel, Inc. All rights reserved.
// $COPYRIGHT$
//
// Additional copyrights may follow
//
// $HEADER$
#ifndef ZEROMQ_TESTS_HPP
#define ZEROMQ_TESTS_HPP

#include "gtest/gtest.h"


#define EXPECT_NULL(x)     EXPECT_EQ((uint64_t)0, (uint64_t)(void*)(x))
#define EXPECT_NOT_NULL(x) EXPECT_NE((uint64_t)0, (uint64_t)(void*)(x))
#define ASSERT_NULL(x)     ASSERT_EQ((uint64_t)0, (uint64_t)(void*)(x))
#define ASSERT_NOT_NULL(x) ASSERT_NE((uint64_t)0, (uint64_t)(void*)(x))
#define EXPECT_PTREQ(x,y)  EXPECT_EQ((uint64_t)(void*)(x), (uint64_t)(void*)(y))
#define EXPECT_PTRNE(x,y)  EXPECT_NE((uint64_t)(void*)(x), (uint64_t)(void*)(y))
#define ASSERT_PTREQ(x,y)  ASSERT_EQ((uint64_t)(void*)(x), (uint64_t)(void*)(y))
#define ASSERT_PTRNE(x,y)  ASSERT_NE((uint64_t)(void*)(x), (uint64_t)(void*)(y))

#include "orcm/mca/db/zeromq/db_zeromq.h"
#include "orcm/mca/db/zeromq/ZeroMQPublisher.hpp"
#include <memory>

extern std::shared_ptr<Publisher> orcm_db_zeromq_object;

#define FAIL_PTR(x) errno_=(x)?EINVAL:0; return (x)?(void*)nullptr:ZeroMQTestClass::POINTER
#define FAIL_INT(x) errno_=(x)?EINVAL:0; return (x)?-1:0


class ZeroMQTestClass : public ZeroMQPublisher
{
public:
    ZeroMQTestClass() : ZeroMQPublisher(), failCtxNew_(false), failCtxTerm_(false),
                        failCtxSet_(false), failSocket_(false), failClose_(false),
                        failSetSocketOpt_(false), failBind_(false), failPublish_(false),
                        failMsgSend_(false), failMsgSend2_(false), failMsgInit_(0),
                        errno_(0), instance_(nullptr) {}
    virtual ~ZeroMQTestClass()
    {
        if(POINTER == socket_) socket_ = NULL;
        if(POINTER == context_) context_ = NULL;
    };

    static void* POINTER;

protected:
    virtual void PublishMessage(const std::string& key, const std::string& message)
    {
        if(failPublish_)
            throw ZeroMQException(-1, "Test exception!");
        else
            ZeroMQPublisher::PublishMessage(key, message);
    };

    virtual void* ZmqCtxNew() { FAIL_PTR(failCtxNew_); };
    virtual int ZmqCtxTerm(void*) { FAIL_INT(failCtxTerm_); };
    virtual int ZmqCtxSet(void*, int, int) { FAIL_INT(failCtxSet_); };
    virtual void* ZmqSocket(void*, int) { FAIL_PTR(failSocket_); };
    virtual int ZmqClose(void*) { FAIL_INT(failClose_); };
    virtual int ZmqSetSocketOpt(void*, int, const void*, size_t) { FAIL_INT(failSetSocketOpt_); };
    virtual int ZmqBind(void*, const char*) { FAIL_INT(failBind_); };
    virtual int ZmqErrno(void) { return errno_; };
    virtual int ZmqMsgSend(zmq_msg_t* m, void*, int f)
    {
        bool more = ((f & ZMQ_SNDMORE) == ZMQ_SNDMORE);
        if((failMsgSend2_ && more) || (failMsgSend_ && !more))
        {
            errno_ = EINVAL;
            return -1;
        }
        else
        {
            errno_ = 0;
            int sz = static_cast<int>(zmq_msg_size(m));
            zmq_msg_close(m);
            return sz;
        }
    };
    virtual int ZmqMsgInitSize(zmq_msg_t* m, size_t sz)
    {
        if(failMsgInit_ == 0)
            return ZeroMQPublisher::ZmqMsgInitSize(m, sz);
        else if(failMsgInit_ == 1)
        {
            errno_ = ENOMEM;
            failMsgInit_--;
            return -1;
        }
        else
        {
            failMsgInit_--;
        return ZeroMQPublisher::ZmqMsgInitSize(m, sz);
        }
    };

public:
    bool failCtxNew_;
    bool failCtxTerm_;
    bool failCtxSet_;
    bool failSocket_;
    bool failClose_;
    bool failSetSocketOpt_;
    bool failBind_;
    bool failPublish_;
    bool failMsgSend_;
    bool failMsgSend2_;
    int failMsgInit_;
    int errno_;
    ZeroMQPublisher* instance_;
};


class BasicTestFixture : public testing::Test
{
protected:
    virtual void SetUp() { orcm_db_zeromq_object = std::shared_ptr<ZeroMQPublisher>(new ZeroMQTestClass()); };
    virtual void TearDown() {};
};

class ZeroMQTestClass2 : public ZeroMQPublisher
{
public:
    ZeroMQTestClass2() : ZeroMQPublisher(), mockAddress_(false) {};

protected:
    virtual std::string BuildAddress(int port)
    {
        if(mockAddress_)
            return std::string("inproc://unittests");
        else
            return ZeroMQPublisher::BuildAddress(port);
    };

public:
    bool mockAddress_;
};


class BasicTestFixture2 : public testing::Test
{
protected:
    virtual void SetUp() { orcm_db_zeromq_object = std::shared_ptr<ZeroMQPublisher>(new ZeroMQTestClass2()); };
    virtual void TearDown() {};
};


class ZeroMQTestClass3 : public ZeroMQPublisher
{
public:
    ZeroMQTestClass3() : ZeroMQPublisher(), mockAddress_(false) {};

protected:
    virtual std::string BuildAddress(int port)
    {
        if(mockAddress_)
            return std::string("garbage");
        else
            return ZeroMQPublisher::BuildAddress(port);
    };

public:
    bool mockAddress_;
};


class BasicTestFixture3 : public testing::Test
{
protected:
    virtual void SetUp() { orcm_db_zeromq_object = std::shared_ptr<ZeroMQPublisher>(new ZeroMQTestClass3()); };
    virtual void TearDown() {};
};

extern "C" {
    #include "opal/dss/dss.h"
    extern int orcm_db_zeromq_component_register(void);
    extern bool orcm_db_zeromq_component_avail(void);
    extern orcm_db_base_module_t* orcm_db_zeromq_component_create(opal_list_t *props);
}

class ComponentTestFixture : public testing::Test
{
protected:
    virtual void SetUp()
    {
        opal_dss_register_vars();
        orcm_db_zeromq_object = std::shared_ptr<ZeroMQPublisher>(new ZeroMQTestClass());
    };
    virtual void TearDown() {};
};

#endif // ZEROMQ_TESTS_HPP
