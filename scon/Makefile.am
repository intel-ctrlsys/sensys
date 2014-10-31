#
# Copyright (c) 2014      Intel, Inc. All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# Build the main SCON library

SUBDIRS = shims

lib_LTLIBRARIES = libscon.la
libscon_la_SOURCES = scon.h scon.c
libscon_la_LIBADD = \
	$(SCON_TOP_BUILDDIR)/orte/mca/rml/libmca_rml.la \
	$(SCON_TOP_BUILDDIR)/orte/mca/oob/libmca_oob.la \
	$(SCON_TOP_BUILDDIR)/orte/mca/routed/libmca_routed.la \
	$(SCON_TOP_BUILDDIR)/opal/lib@OPAL_LIB_PREFIX@open-pal.la
libscon_la_DEPENDENCIES = $(lib@ORTE_LIB_PREFIX@open_rte_la_LIBADD)
libscon_la_LDFLAGS = -version-info $(libscon_so_version)

