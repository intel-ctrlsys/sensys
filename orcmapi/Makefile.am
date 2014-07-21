#
# Copyright (c) 2014      Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# Build the main orcmapi library

lib_LTLIBRARIES = liborcmapi.la
liborcmapi_la_SOURCES = orcmapi.h orcmapi.c
liborcmapi_la_LIBADD = $(ORCMAPI_TOP_BUILDDIR)/orcm/liborcm.la
liborcmapi_la_DEPENDENCIES =
#FIX: library versioning doesn't work, but fix this workaround
liborcmapi_la_LDFLAGS = -avoid-version
#liborcmapi_la_LDFLAGS = -version-info $(liborcmapi_so_version)

