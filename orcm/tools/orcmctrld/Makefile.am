#
# Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved. 
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_orcmdata_DATA = help-orcmctrld.txt

include $(top_srcdir)/Makefile.ompi-rules

man_pages = orcmctrld.1
EXTRA_DIST = $(man_pages:.1=.1in)

if OPAL_INSTALL_BINARIES

bin_PROGRAMS = orcmctrld

nodist_man_MANS = $(man_pages)

# Ensure that the man pages are rebuilt if the orcm_config.h file
# changes; a "good enough" way to know if configure was run again (and
# therefore the release date or version may have changed)
$(nodist_man_MANS): $(top_builddir)/orcm/include/orcm_config.h

endif # OPAL_INSTALL_BINARIES

orcmctrld_SOURCES = orcmctrld.c
# the following empty orcmd_LDFLAGS is used 
#  so that the orcmd can be compiled statically 
#  by simply changing the value of this from 
#  nothing to -all-static in the Makefile.in
#  nice for systems that don't have all the shared 
#  libraries on the computes 
orcmctrld_LDFLAGS =
orcmctrld_LDADD = $(top_builddir)/orcm/liborcm.la $(top_builddir)/orte/libopen-rte.la

distclean-local:
	rm -f $(man_pages)

