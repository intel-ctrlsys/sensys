/*
 * Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved.
 * Copyright (c) 2014-2016 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 *
 * Additional copyrights may follow
 *
 * $HEADER$
 */
/** @file:
 *
 * The OPAL Database Framework
 *
 */

#ifndef OPAL_DSTORE_TYPES_H
#define OPAL_DSTORE_TYPES_H

#include "opal_config.h"
#include "opal/types.h"

#include "opal/dss/dss_types.h"
#include "opal/mca/pmix/pmix.h"

BEGIN_C_DECLS

/* some values are provided by an external entity such
 * as the resource manager. These values enter the
 * system via the PMIx interface at startup, but are
 * not explicitly retrieved by processes. Instead, procs
 * access them after RTE-init has stored them. For ease-of-use,
 * we define equivalent dstore names here. PMIx attributes
 * not listed here should be directly accessed via the
 * OPAL pmix framework */
#define OPAL_DSTORE_CPUSET        PMIX_CPUSET
#define OPAL_DSTORE_CREDENTIAL    PMIX_CREDENTIAL
#define OPAL_DSTORE_NODERANK      PMIX_NODE_RANK
#define OPAL_DSTORE_HOSTNAME      PMIX_HOSTNAME

/* some OPAL-appropriate key definitions */
#define OPAL_DSTORE_LOCALITY      "opal.locality"          // (uint16_t) relative locality of a peer
/* proc-specific scratch dirs */
#define OPAL_DSTORE_JOB_SDIR      "opal.job.session.dir"  // (char*) job-level session dir
#define OPAL_DSTORE_MY_SDIR       "opal.my.session.dir"   // (char*) session dir for this proc
#define OPAL_DSTORE_URI           "opal.uri"              // (char*) uri of specified proc
#define OPAL_DSTORE_ARCH          "opal.arch"             // (uint32_t) arch for specified proc

END_C_DECLS

#endif
