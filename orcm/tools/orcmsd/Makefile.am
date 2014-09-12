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
# Copyright (c) 2007-2014 Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2008      Sun Microsystems, Inc.  All rights reserved.
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_orcmdata_DATA = help-orcmsd.txt

include $(top_srcdir)/Makefile.ompi-rules

man_pages = orcmsd.1
EXTRA_DIST = $(man_pages:.1=.1in)

if OPAL_INSTALL_BINARIES

bin_PROGRAMS = orcmsd

nodist_man_MANS = $(man_pages)

# Ensure that the man pages are rebuilt if the opal_config.h file
# changes; a "good enough" way to know if configure was run again (and
# therefore the release date or version may have changed)
$(nodist_man_MANS): $(top_builddir)/opal/include/opal_config.h

endif # OPAL_INSTALL_BINARIES

orcmsd_SOURCES = orcmsd.c
# the following empty orcmsd_LDFLAGS is used 
#  so that the orcmsd can be compiled statically 
#  by simply changing the value of this from 
#  nothing to -all-static in the Makefile.in
#  nice for systems that don't have all the shared 
#  libraries on the computes 
orcmsd_LDFLAGS =
orcmsd_LDADD = $(top_builddir)/orcm/liborcm.la $(top_builddir)/orte/libopen-rte.la $(top_builddir)/opal/libopen-pal.la

distclean-local:
	rm -f $(man_pages)
