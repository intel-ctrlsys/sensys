#
# Copyright (c) 2015      Intel, Inc.  All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# main library setup
noinst_LTLIBRARIES = libmca_evgen.la
libmca_evgen_la_SOURCES =

# local files
headers = evgen.h \
          evgen_types.h

libmca_evgen_la_SOURCES += $(headers)

# Conditionally install the header files
if WANT_INSTALL_HEADERS
orcmdir = $(orcmincludedir)/$(subdir)
nobase_orcm_HEADERS = $(headers)
endif

include base/Makefile.am

distclean-local:
	rm -f base/static-components.h
