#
# Copyright (c) 2014      Intel, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

if OPAL_INSTALL_BINARIES

bin_PROGRAMS = orcm-emulator


dist_orcmdata_DATA = help-emulator.txt

endif # OPAL_INSTALL_BINARIES

orcm_emulator_SOURCES = \
        orcm-emulator.c

orcm_emulator_LDFLAGS =
orcm_emulator_LDADD = $(top_builddir)/orcm/liborcm.la $(top_builddir)/orte/libopen-rte.la

distclean-local:
	rm -f $(man_pages)
