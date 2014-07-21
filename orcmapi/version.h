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
 * version information for ORCMAPI
 */

#ifndef ORCMAPI_VERSIONS_H
#define ORCMAPI_VERSIONS_H

#define ORCMAPI_MAJOR_VERSION 1
#define ORCMAPI_MINOR_VERSION 0
#define ORCMAPI_RELEASE_VERSION 0
#define ORCMAPI_GREEK_VERSION "a1"
#define ORCMAPI_WANT_REPO_REV 1
#define ORCMAPI_REPO_REV "r32231M"
#ifdef ORCMAPI_VERSION
/* If we included version.h, we want the real version, not the
   stripped (no-r number) version */
#undef ORCMAPI_VERSION
#endif
#define ORCMAPI_VERSION "1.0a1r32231M"

#endif
