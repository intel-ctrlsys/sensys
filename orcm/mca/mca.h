/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil -*- */
/*
 * Copyright (c) 2004-2008 The Trustees of Indiana University and Indiana
 *                         University Research and Technology
 *                         Corporation.  All rights reserved.
 * Copyright (c) 2004-2005 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 * Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
 *                         University of Stuttgart.  All rights reserved.
 * Copyright (c) 2004-2005 The Regents of the University of California.
 *                         All rights reserved.
 * Copyright (c) 2008-2012 Cisco Systems, Inc.  All rights reserved.
 * Copyright (c) 2015      Los Alamos National Security, LLC. All rights
 *                         reserved.
 * Copyright (c) 2015      Intel Corporation. All rights reserved.
 * Copyright (c) 2016      Intel Corporation. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 */
/** 
 * @file 
 *
 * Top-level interface for \em all orcm MCA components.
 */

#ifndef ORCM_MCA_H
#define ORCM_MCA_H

#include "orcm_config.h"
#include "opal/mca/mca.h"

#define ORCM_MCA_BASE_VERSION_2_1_0(type, type_major, type_minor, type_release) \
    MCA_BASE_VERSION_2_1_0("orcm", ORCM_MAJOR_VERSION, ORCM_MINOR_VERSION,      \
                           ORCM_RELEASE_VERSION, type, type_major, type_minor,  \
                           type_release)

#endif /* ORCM_MCA_H */
