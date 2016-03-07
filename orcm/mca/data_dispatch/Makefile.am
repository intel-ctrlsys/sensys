#
# Copyright (c) 2015-2016      Intel, Inc.  All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# main library setup
noinst_LTLIBRARIES = libmca_data_dispatch.la
libmca_data_dispatch_la_SOURCES =

# local files
headers = data_dispatch.h \
          data_dispatch_types.h

libmca_data_dispatch_la_SOURCES += $(headers)

# Conditionally install the header files
if WANT_INSTALL_HEADERS
orcmdir = $(orcmincludedir)/$(subdir)
nobase_orcm_HEADERS = $(headers)
endif

include base/Makefile.am

distclean-local:
	rm -f base/static-components.h
