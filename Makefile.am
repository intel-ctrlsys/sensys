#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2009 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2006-2015 Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2012-2013 Los Alamos National Security, Inc.  All rights reserved.
# Copyright (c) 2014-2015 Intel, Inc. All rights reserved
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

SUBDIRS = config contrib $(MCA_PROJECT_SUBDIRS) scon
EXTRA_DIST = README INSTALL VERSION Doxyfile LICENSE autogen.pl

dist-hook:
	env LS_COLORS= sh "$(top_srcdir)/config/distscript.sh" "$(top_srcdir)" "$(distdir)" "$(ORCM_VERSION)" "$(ORCM_REPO_REV)"

ACLOCAL_AMFLAGS = -I config
