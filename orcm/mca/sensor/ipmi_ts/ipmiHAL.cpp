/*
 * Copyright (c) 2016 Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */

#include "orcm/mca/sensor/ipmi_ts/ipmiHAL.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiutilDFx.h"
#include "orcm/mca/sensor/ipmi_ts/ipmiutilAgent.h"

#include <cstddef>
#include <string>
#include <sstream>
#include <ctime>

extern "C" {
    #include "opal/mca/event/event.h"
    #include "opal/runtime/opal_progress_threads.h"
    #include "orcm/util/utils.h"
    #include <stdlib.h>
}

using namespace std;

/*
 * The following are OPAL objects, and they are being treated as static variables within this file
 * Sadly, even if this is a bad practice, it is necessary given the legacy code we need to work with
 */

// Constants
static const struct timeval CONSUMER_RATE = {0, 100};
static const char THREAD_NAME[] = "ipmiHAL";
static const char MAX_NUM_OF_DISPATCH_AGENTS = 100;
static const int DEFAULT_AGENTS = 4;

// Threads and queues
static opal_event_t *consumerHandler = NULL;
static opal_event_base_t *ev_base = NULL;
static opal_event_base_t **dispatchingThreads = NULL;
static ipmiLibInterface *ptrToAgent = NULL;
static int currentThread = 0;
static int ev_count = 0; //This counter is for testing purposes only. Do not rely on it.

// Prototypes to functions
static bool shouldUseDFx_();
static int getNumberOfDispatchingAgents();
static void processRequest_(int, short, void* ptr);
void dispatchResponseToCallback_(int, short, void* ptr);

// Aux data structure
typedef struct
{
    ipmiCommands command;
    buffer data;
    string bmc;
    ipmiHAL_callback cbFunction;
    void* cbData;
    ipmiResponse response;
    opal_event_t *handler;
} request_data_t;


ipmiHAL* ipmiHAL::s_instance = 0;
ipmiLibInterface* ipmiHAL::agent = 0;

ipmiHAL::ipmiHAL() : consuming(false)
{
    if (shouldUseDFx_())
        agent = new ipmiutilDFx();
    else
        agent = new ipmiutilAgent("");

    ptrToAgent = agent;
}

ipmiHAL::~ipmiHAL()
{
    finalizeThreads_();
    releaseHandlers_();

    ptrToAgent = NULL;
    delete agent;
}

void ipmiHAL::finalizeThreads_()
{
    opal_progress_thread_finalize(THREAD_NAME);

    int numAgents = getNumberOfDispatchingAgents();
    if (numAgents > MAX_NUM_OF_DISPATCH_AGENTS)
        numAgents = MAX_NUM_OF_DISPATCH_AGENTS;

    for (int i = 0; i < numAgents; ++i)
        opal_progress_thread_finalize(getThreadName_(i));
    delete  dispatchingThreads;
    dispatchingThreads = NULL;
}

void ipmiHAL::releaseHandlers_()
{
    if (NULL != consumerHandler)
    {
        opal_event_del(consumerHandler);
        opal_event_free(consumerHandler);
        consumerHandler = NULL;
    }
}

ipmiHAL* ipmiHAL::getInstance()
{
    if (!s_instance) {
        s_instance = new ipmiHAL();

        ev_base = opal_progress_thread_init(THREAD_NAME);
        throwWhenNullPointer(ev_base);
        ev_count = 0;

        if (!shouldUseDFx_())
            s_instance->startAgents();
    }
    return s_instance;
}

void ipmiHAL::startAgents()
{
    initialize_();
}

void ipmiHAL::addRequest(ipmiCommands command, buffer data, string bmc, ipmiHAL_callback cbFunction, void* cbData)
{
    request_data_t *request_item = new request_data_t();
    request_item->command = command;
    request_item->data = data;
    request_item->bmc = bmc;
    request_item->cbFunction = cbFunction;
    request_item->cbData = cbData;
    request_item->handler = opal_event_evtimer_new(ev_base,
                                                   processRequest_,
                                                   request_item);

    if (NULL == request_item->handler)
        throwWhenNullPointer(request_item->handler);
    else
        opal_event_add(request_item->handler, &CONSUMER_RATE);
    ++ev_count;
}

// This function is for testing purposes. Do not rely on it.
bool ipmiHAL::isQueueEmpty()
{
    return 0 == ev_count;
}

void ipmiHAL::initialize_()
{
    initializeDispatchingAgents_();
    initializeConsumerLoop_();
    consuming = true;
}

void ipmiHAL::initializeConsumerLoop_()
{
    if (consuming)  // Already initialized, no need for initialization
        return;


    consumerHandler = opal_event_evtimer_new(ev_base, processRequest_, NULL);
    throwWhenNullPointer(consumerHandler);
}

void ipmiHAL::terminateInstance()
{
    if (s_instance) {
        delete s_instance;
        s_instance = NULL;
    }
}

void ipmiHAL::initializeDispatchingAgents_()
{
    if (consuming)
        return;

    int n = DEFAULT_AGENTS;
    n = getNumberOfDispatchingAgents();
    if (n > MAX_NUM_OF_DISPATCH_AGENTS)
        n = MAX_NUM_OF_DISPATCH_AGENTS;
    initializeDispatchThreads_(n);
}

const char* ipmiHAL::getThreadName_(int index)
{
    static string baseName("ipmiHAL_dispatcher_");
    string number = static_cast<ostringstream*>( &(ostringstream() << index) )->str();
    return (baseName + number).c_str();
}

void ipmiHAL::initializeDispatchThreads_(int n)
{
    dispatchingThreads = new opal_event_base_t*[n];
    for (int i = 0; i < n; ++i)
        dispatchingThreads[i] = opal_progress_thread_init(getThreadName_(i));
}

void ipmiHAL::throwWhenNullPointer(void* ptr)
{
    if (NULL == ptr)
        throw ipmiHAL_objects::unableToAllocateObj();
}

/*
 * Functions with legacy code:
 */
int getNumberOfDispatchingAgents()
{
    static const char MCA_AGENTS_EVAR[] = "ORCM_MCA_sensor_ipmi_ts_agents";

    int agents = DEFAULT_AGENTS;
    char* envValue = getenv(MCA_AGENTS_EVAR);

    if (NULL != envValue)
    {
        envValue = strdup(envValue);
        if (NULL != envValue)
        {
            agents = atoi(envValue);
            free(envValue);
        }
    }

    return agents <= 0 ? DEFAULT_AGENTS : agents;
}

bool shouldUseDFx_()
{
    static const char MCA_DFX_FLAG[] = "ORCM_MCA_sensor_ipmi_ts_dfx";
    char *envShouldUseDfx = getenv(MCA_DFX_FLAG);

    if (NULL == envShouldUseDfx)
        return false;

    if (0 == string(envShouldUseDfx).compare("1"))
        return true;

    return false;
}

void processRequest_(int, short, void* ptr)
{
    request_data_t *request_item = (request_data_t*) ptr;

    if (NULL != request_item) {
        request_item->response= ptrToAgent->sendCommand(request_item->command, &request_item->data, request_item->bmc);

        opal_event_free(request_item->handler);
        --ev_count;

        request_item->handler = opal_event_evtimer_new(dispatchingThreads[currentThread],
                                                       dispatchResponseToCallback_,
                                                       request_item);
        if (NULL != request_item->handler) {
            opal_event_add(request_item->handler, &CONSUMER_RATE);
            currentThread = (currentThread+1) % getNumberOfDispatchingAgents();
        } else {
            delete request_item;
            throw ipmiHAL_objects::unableToAllocateObj();
        }
    }
}

void dispatchResponseToCallback_(int, short, void* ptr)
{
    request_data_t *request_item = (request_data_t *) ptr;

    if (NULL != request_item->cbFunction)
        request_item->cbFunction(request_item->bmc, request_item->response, request_item->cbData);
    opal_event_free(request_item->handler);
    delete request_item;
}
