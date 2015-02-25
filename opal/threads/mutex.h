/*
 * Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2006 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2007      Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2007-2013 Los Alamos National Security, LLC.  All rights
 *                         reserved. 
 * Copyright (c) 2007      Voltaire. All rights reserved.
 * Copyright (c) 2010      Oracle and/or its affiliates.  All rights reserved.
 *
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */

#ifndef  OPAL_MUTEX_H
#define  OPAL_MUTEX_H 1

#include "opal_config.h"

#include "opal/sys/atomic.h"

BEGIN_C_DECLS

/**
 * @file:
 *
 * Mutual exclusion functions.
 *
 * Functions for locking of critical sections.
 */

/**
 * Opaque mutex object
 */
typedef struct opal_mutex_t opal_mutex_t;


/**
 * Try to acquire a mutex.
 *
 * @param mutex         Address of the mutex.
 * @return              0 if the mutex was acquired, 1 otherwise.
 */
static inline int opal_mutex_trylock(opal_mutex_t *mutex);


/**
 * Acquire a mutex.
 *
 * @param mutex         Address of the mutex.
 */
static inline void opal_mutex_lock(opal_mutex_t *mutex);


/**
 * Release a mutex.
 *
 * @param mutex         Address of the mutex.
 */
static inline void opal_mutex_unlock(opal_mutex_t *mutex);


/**
 * Try to acquire a mutex using atomic operations.
 *
 * @param mutex         Address of the mutex.
 * @return              0 if the mutex was acquired, 1 otherwise.
 */
static inline int opal_mutex_atomic_trylock(opal_mutex_t *mutex);


/**
 * Acquire a mutex using atomic operations.
 *
 * @param mutex         Address of the mutex.
 */
static inline void opal_mutex_atomic_lock(opal_mutex_t *mutex);


/**
 * Release a mutex using atomic operations.
 *
 * @param mutex         Address of the mutex.
 */
static inline void opal_mutex_atomic_unlock(opal_mutex_t *mutex);

END_C_DECLS

#include "mutex_unix.h"

BEGIN_C_DECLS

static inline bool opal_using_threads(void)
{
    return 0;
}

/**
 * Set whether the process is using multiple threads or not.
 *
 * @param have Boolean indicating whether the process is using
 * multiple threads or not.
 *
 * @retval opal_using_threads The new return value from
 * opal_using_threads().
 *
 * This function is used to influence the return value of
 * opal_using_threads().  If configure detected that we have thread
 * support, the return value of future invocations of
 * opal_using_threads() will be the parameter's value.  If configure
 * detected that we have no thread support, then the retuen from
 * opal_using_threads() will always be false.
 */
static inline bool opal_set_using_threads(bool have)
{
    if(have) {}; //Bogus use of have to stop compiler error for an orcm test.
    return opal_using_threads();
}


/**
 * Lock a mutex if opal_using_threads() says that multiple threads may
 * be active in the process.
 *
 * @param mutex Pointer to a opal_mutex_t to lock.
 *
 * If there is a possibility that multiple threads are running in the
 * process (as determined by opal_using_threads()), this function will
 * block waiting to lock the mutex.
 *
 * If there is no possibility that multiple threads are running in the
 * process, return immediately.
 */
#define OPAL_THREAD_LOCK(mutex)


/**
 * Try to lock a mutex if opal_using_threads() says that multiple
 * threads may be active in the process.
 *
 * @param mutex Pointer to a opal_mutex_t to trylock
 *
 * If there is a possibility that multiple threads are running in the
 * process (as determined by opal_using_threads()), this function will
 * trylock the mutex.
 *
 * If there is no possibility that multiple threads are running in the
 * process, return immediately without modifying the mutex.
 *
 * Returns 0 if mutex was locked, non-zero otherwise.
 */
#define OPAL_THREAD_TRYLOCK(mutex) 0

/**
 * Unlock a mutex if opal_using_threads() says that multiple threads
 * may be active in the process.
 *
 * @param mutex Pointer to a opal_mutex_t to unlock.
 *
 * If there is a possibility that multiple threads are running in the
 * process (as determined by opal_using_threads()), this function will
 * unlock the mutex.
 *
 * If there is no possibility that multiple threads are running in the
 * process, return immediately without modifying the mutex.
 */
#define OPAL_THREAD_UNLOCK(mutex)


/**
 * Lock a mutex if opal_using_threads() says that multiple threads may
 * be active in the process for the duration of the specified action.
 *
 * @param mutex    Pointer to a opal_mutex_t to lock.
 * @param action   A scope over which the lock is held.
 *
 * If there is a possibility that multiple threads are running in the
 * process (as determined by opal_using_threads()), this function will
 * acquire the lock before invoking the specified action and release
 * it on return.
 *
 * If there is no possibility that multiple threads are running in the
 * process, invoke the action without acquiring the lock.
 */
#define OPAL_THREAD_SCOPED_LOCK(mutex, action)  \
        (action);

/**
 * Use an atomic operation for increment/decrement if opal_using_threads()
 * indicates that threads are in use by the application or library.
 */

static inline int32_t
OPAL_THREAD_ADD32(volatile int32_t *addr, int delta)
{
    int32_t ret;

    ret = (*addr += delta);

    return ret;
}

#if OPAL_HAVE_ATOMIC_MATH_64
static inline int64_t
OPAL_THREAD_ADD64(volatile int64_t *addr, int delta)
{
    int64_t ret;

    ret = (*addr += delta);

    return ret;
}
#endif

static inline size_t
OPAL_THREAD_ADD_SIZE_T(volatile size_t *addr, int delta)
{
    size_t ret;

    ret = (*addr += delta);

    return ret;
}

/* BWB: FIX ME: remove if possible */
#define OPAL_CMPSET(x, y, z) ((*(x) == (y)) ? ((*(x) = (z)), 1) : 0)

#if OPAL_HAVE_ATOMIC_CMPSET_32
#define OPAL_ATOMIC_CMPSET_32(x, y, z) \
    OPAL_CMPSET(x, y, z)
#endif
#if OPAL_HAVE_ATOMIC_CMPSET_64
#define OPAL_ATOMIC_CMPSET_64(x, y, z) \
    OPAL_CMPSET(x, y, z)
#endif
#if OPAL_HAVE_ATOMIC_CMPSET_32 || OPAL_HAVE_ATOMIC_CMPSET_64
#define OPAL_ATOMIC_CMPSET(x, y, z) \
    OPAL_CMPSET(x, y, z)
#endif

END_C_DECLS

#endif                          /* OPAL_MUTEX_H */
