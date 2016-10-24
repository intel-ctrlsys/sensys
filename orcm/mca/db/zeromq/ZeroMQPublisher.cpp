//
// Copyright (c) 2016      Intel Corporation. All rights reserved.
// $COPYRIGHT$
//
// Additional copyrights may follow
//
// $HEADER$
//
#include "ZeroMQPublisher.hpp"

#include <stdexcept>
#include <sstream>
#include <new>

#include <memory.h>
#include <iostream>

// API
void ZeroMQPublisher::Init(int port, int threadsHint, int maxBuffersHint, OuputCallbackFuncPtr callback)
{
    output_ = callback;
    if(0 == initialized_)
    {
        CreateContext(threadsHint);
        CreateSocket(port, maxBuffersHint);
    }
    ++initialized_;
}

void ZeroMQPublisher::Finalize()
{
    if(0 < initialized_)
    {
        --initialized_;
        if(0 == initialized_)
            CloseSocketAndContext();
    }
}

void ZeroMQPublisher::PublishMessage(const std::string& key, const std::string& message)
{
    if(socket_)
    {
        zmq_msg_t k;
        zmq_msg_t m;
        if(-1 == ZmqMsgInitSize(&k, key.size()))
            throw std::bad_alloc();
        if(-1 == ZmqMsgInitSize(&m, message.size()))
            throw std::bad_alloc();
        memcpy(zmq_msg_data(&k), key.data(), key.size());
        memcpy(zmq_msg_data(&m), message.data(), message.size());
        if(static_cast<int>(key.size()) == ZmqMsgSend(&k, socket_, ZMQ_SNDMORE))
            if(static_cast<int>(message.size()) == ZmqMsgSend(&m, socket_, 0))
                return;
            else
            {
                zmq_msg_close(&m);
                throw ZeroMQException(ZmqErrno(), "Failed to send the message frame of the message!");
            }
        else
        {
            zmq_msg_close(&m);
            zmq_msg_close(&k);
            throw ZeroMQException(ZmqErrno(), "Failed to send the key frame of the message!");
        }
    }
    else
        throw ZeroMQException(0, "ZeroMQPublisher is not initialized!");
}

void ZeroMQPublisher::SetOutputCallback(OuputCallbackFuncPtr callback)
{
    output_ = callback;
}

void ZeroMQPublisher::Output(int level, const char* message)
{
    if(nullptr != output_)
        output_(level, message);
}


// Implementation
void ZeroMQPublisher::CloseSocketAndContext()
{
    CloseSocket();
    CloseContext();
}

void ZeroMQPublisher::CloseContext()
{
    if(nullptr != context_)
    {
        ZmqCtxTerm(context_);
        context_ = nullptr;
    }
}

void ZeroMQPublisher::CloseSocket()
{
    if(nullptr != socket_)
    {
        ZmqClose(socket_);
        socket_ = nullptr;
    }
}

void ZeroMQPublisher::CleanupAndThrow(const std::string& msg)
{
    CloseSocketAndContext();
    throw ZeroMQException(ZmqErrno(), msg.c_str());
}

void ZeroMQPublisher::OnNullPtrCleanupAndThrow(void* ptr, const std::string& message)
{
    if(nullptr == ptr)
        CleanupAndThrow(message);
}

void ZeroMQPublisher::OnErrorCleanupAndThrow(int result, const std::string& message)
{
    if(result)
        CleanupAndThrow(message);
}

void ZeroMQPublisher::CreateContext(int threadsHint)
{
    context_ = ZmqCtxNew();
    OnNullPtrCleanupAndThrow(context_, "Failed to create a ZeroMQ context!");
    int rv = ZmqCtxSet(context_, ZMQ_IO_THREADS, threadsHint);
    OnErrorCleanupAndThrow(rv, "Failed to set threads count into the ZeroMQ context!");
}

void ZeroMQPublisher::CreateSocket(int port, int maxBuffersHint)
{
    socket_ = ZmqSocket(context_, ZMQ_PUB);
    OnNullPtrCleanupAndThrow(socket_, "Failed to the ZeroMQ publisher socket!");
    int rv = ZmqSetSocketOpt(socket_, ZMQ_SNDHWM, &maxBuffersHint, sizeof(int));
    OnErrorCleanupAndThrow(rv, "Failed to the ZeroMQ publisher socket!");
    rv = ZmqBind(socket_, BuildAddress(port).c_str());
    OnErrorCleanupAndThrow(rv, "Failed to bind the ZeroMQ publisher socket!");
}

std::string ZeroMQPublisher::BuildAddress(int port)
{
    std::stringstream ss;
    ss << "tcp://*:" << port;
    Output(1, std::string("ZeroMQ publisher binding to " + ss.str()).c_str());
    return ss.str();
}


// MockableZeroMQPublisher:: ZeroMQ APIs

void* ZeroMQPublisher::ZmqCtxNew(void) { return zmq_ctx_new(); }
int ZeroMQPublisher::ZmqCtxTerm(void* ctx) { return zmq_ctx_term(ctx); }
int ZeroMQPublisher::ZmqCtxSet(void* ctx, int option, int value) { return zmq_ctx_set(ctx, option, value); }
void* ZeroMQPublisher::ZmqSocket(void* ctx, int type) { return zmq_socket(ctx, type); }
int ZeroMQPublisher::ZmqClose(void* socket) { return zmq_close(socket); }
int ZeroMQPublisher::ZmqSetSocketOpt(void* socket, int option, const void* value, size_t valueLen)
    { return zmq_setsockopt(socket, option, value, valueLen); }
int ZeroMQPublisher::ZmqBind(void* socket, const char* address) { return zmq_bind(socket, address); }
int ZeroMQPublisher::ZmqErrno(void) { return zmq_errno(); }
int ZeroMQPublisher::ZmqMsgSend(zmq_msg_t* msg, void* socket, int flags) { return zmq_msg_send(msg, socket, flags); }
int ZeroMQPublisher::ZmqMsgInitSize(zmq_msg_t* m, size_t sz) { return zmq_msg_init_size(m, sz); }
