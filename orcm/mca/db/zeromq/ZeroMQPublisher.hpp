//
// Copyright (c) 2016      Intel Corporation. All rights reserved.
// $COPYRIGHT$
//
// Additional copyrights may follow
//
// $HEADER$
//
#ifndef PUBSUB_ZEROMQPUBLISHER_HPP
#define PUBSUB_ZEROMQPUBLISHER_HPP

#include "Publisher.hpp"

#include <stdexcept>

#include <zmq.h>

class ZeroMQException : public std::runtime_error
{
public:
    ZeroMQException(int error, const std::string& msg) : std::runtime_error(msg.c_str()), error_(error) {};
    int ZMQError() const { return error_; };
private:
    int error_;
};

class ZeroMQPublisher : public Publisher
{
public:
    ZeroMQPublisher() : initialized_(0), context_(NULL), socket_(NULL), output_(NULL) {};
    virtual ~ZeroMQPublisher() { Finalize(); };

    virtual void Init(int port, int threadsHint = 1, int maxBuffersHint = 10000, OuputCallbackFuncPtr callback = NULL);
    virtual void Finalize();
    virtual void PublishMessage(const std::string& key, const std::string& message);
    virtual void SetOutputCallback(OuputCallbackFuncPtr callback);
    virtual void Output(int level, const char* message);

private: // Methods
    void CleanupAndThrow(const std::string& msg);
    void CloseSocketAndContext();
    void CloseContext();
    void CloseSocket();
    void OnNullPtrCleanupAndThrow(void* ptr, const std::string& message);
    void OnErrorCleanupAndThrow(int result, const std::string& message);
    void CreateContext(int threadsHint);
    void CreateSocket(int port, int maxBuffersHint);

protected: // Methods for Mocking
    virtual std::string BuildAddress(int port);

    virtual void* ZmqCtxNew(void);
    virtual int ZmqCtxTerm(void* ctx);
    virtual int ZmqCtxSet(void* ctx, int option, int value);
    virtual void* ZmqSocket(void* ctx, int type);
    virtual int ZmqClose(void* socket);
    virtual int ZmqSetSocketOpt(void* socket, int option, const void* value, size_t valueLen);
    virtual int ZmqBind(void* socket, const char* address);
    virtual int ZmqMsgSend(zmq_msg_t* msg, void* socket, int flags);
    virtual int ZmqErrno(void);
    virtual int ZmqMsgInitSize(zmq_msg_t* m, size_t sz);

protected: // Fields
    int initialized_;
    void* context_;
    void* socket_;
    OuputCallbackFuncPtr output_;

};
#endif //PUBSUB_ZEROMQPUBLISHER_HPP
