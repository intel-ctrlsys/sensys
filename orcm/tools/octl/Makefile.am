#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_orcmdata_DATA = help-octl.txt

include $(top_srcdir)/Makefile.ompi-rules

man_pages = octl.1
EXTRA_DIST = $(man_pages:.1=.1in)

if OPAL_INSTALL_BINARIES

bin_PROGRAMS = octl

nodist_man_MANS = $(man_pages)

# Ensure that the man pages are rebuilt if the orcm_config.h file
# changes; a "good enough" way to know if configure was run again (and
# therefore the release date or version may have changed)
$(nodist_man_MANS): $(top_builddir)/orcm/include/orcm_config.h

endif # OPAL_INSTALL_BINARIES

octl_SOURCES = \
        common.h \
        diag.c \
        octl.h \
        octl.c \
        queue.c \
        resource.c \
        session.c

# the following empty octl_LDFLAGS is used
#  so that the octl can be compiled statically
#  by simply changing the value of this from 
#  nothing to -all-static in the Makefile.in
#  nice for systems that don't have all the shared 
#  libraries on the computes 
octl_LDFLAGS =
octl_LDADD = $(top_builddir)/orcm/liborcm.la $(top_builddir)/orte/libopen-rte.la

distclean-local:
	rm -f $(man_pages)
