#
# Copyright (c) 2015-2016      Intel Corporation.  All rights reserved.
#
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# main library setup
noinst_LTLIBRARIES = libmca_dispatch.la
libmca_dispatch_la_SOURCES =

# local files
headers = dispatch.h \
          dispatch_types.h

libmca_dispatch_la_SOURCES += $(headers)

# Conditionally install the header files
if WANT_INSTALL_HEADERS
orcmdir = $(orcmincludedir)/$(subdir)
nobase_orcm_HEADERS = $(headers)
endif

include base/Makefile.am

distclean-local:
	rm -f base/static-components.h
