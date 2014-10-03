#
# Copyright (c) 2014      Intel, Inc. All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# Build the mrnet shim library

lib_LTLIBRARIES = libmrnetscon.la
libmrnetscon_la_SOURCES = mrnetscon.h mrnetscon.c
libmrnetscon_la_LIBADD = \
	$(SCON_TOP_BUILDDIR)/orte/mca/rml/libmca_rml.la \
	$(SCON_TOP_BUILDDIR)/orte/mca/oob/libmca_oob.la \
	$(SCON_TOP_BUILDDIR)/orte/mca/routed/libmca_routed.la \
	$(SCON_TOP_BUILDDIR)/opal/libopen-pal.la
libmrnetscon_la_DEPENDENCIES = $(libopen_rte_la_LIBADD)
libmrnetscon_la_LDFLAGS = -version-info $(libscon_so_version)

