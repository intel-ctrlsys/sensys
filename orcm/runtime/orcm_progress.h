/*
 * Copyright (c) 2014      Intel, Inc.  All rights reserved. 
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef ORCM_PROGRESS_H
#define ORCM_PROGRESS_H

#include "orcm_config.h"

#include "opal/mca/event/event.h"

/* start a progress thread using the given function, assigning
 * it the provided name for tracking purposes. This function will
 * also create a pipe so that libevent has something to block
 * against, thus keeping the thread from free-running
 */
ORCM_DECLSPEC opal_event_base_t *orcm_start_progress_thread(char *name, opal_thread_fn_t func, void* args);

/* stop the progress thread of the provided name. This function will
 * also cleanup the blocking pipes and release the event base if
 * the cleanup param is true */
ORCM_DECLSPEC void orcm_stop_progress_thread(char *name, bool cleanup);

/* restart the progress thread of the provided name */
ORCM_DECLSPEC int orcm_restart_progress_thread(char *name);

#endif
