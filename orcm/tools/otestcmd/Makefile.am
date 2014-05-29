#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

if OPAL_INSTALL_BINARIES

bin_PROGRAMS = otestcmd

endif # OPAL_INSTALL_BINARIES

otestcmd_SOURCES = otestcmd.c
# the following empty otestcmd_LDFLAGS is used 
#  so that the otestcmd can be compiled statically 
#  by simply changing the value of this from 
#  nothing to -all-static in the Makefile.in
#  nice for systems that don't have all the shared 
#  libraries on the computes 
otestcmd_LDFLAGS =
otestcmd_LDADD = $(top_builddir)/orcm/liborcm.la $(top_builddir)/orte/libopen-rte.la

distclean-local:
	rm -f $(man_pages)
