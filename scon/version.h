/* -*- c -*-
 *
 * Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
 * $COPYRIGHT$
 * 
 * Additional copyrights may follow
 * 
 * $HEADER$
 *
 * This file should be included by any file that needs full
 * version information for the SCON project
 */

#ifndef SCON_VERSIONS_H
#define SCON_VERSIONS_H

#define SCON_MAJOR_VERSION 1
#define SCON_MINOR_VERSION 0
#define SCON_RELEASE_VERSION 0
#define SCON_GREEK_VERSION "a1"
#define SCON_WANT_REPO_REV 1
#define SCON_REPO_REV "r32231M"
#ifdef SCON_VERSION
/* If we included version.h, we want the real version, not the
   stripped (no-r number) verstion */
#undef SCON_VERSION
#endif
#define SCON_VERSION "1.0a1r32231M"

#endif
