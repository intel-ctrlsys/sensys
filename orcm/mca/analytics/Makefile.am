#
# Copyright (c) 2014-2016      Intel Corporation.  All rights reserved.
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# main library setup
noinst_LTLIBRARIES = libmca_analytics.la
libmca_analytics_la_SOURCES =

# local files
headers = analytics.h \
          analytics_types.h \
          analytics_interface.h

libmca_analytics_la_SOURCES += $(headers)
libmca_analytics_la_CXXFLAGS = -std=c++11

# Conditionally install the header files
if WANT_INSTALL_HEADERS
orcmdir = $(includedir)/openmpi/$(subdir)
nobase_orcm_HEADERS = $(headers)
endif

include base/Makefile.am
include extension_plugins/average/Makefile.am

distclean-local:
	rm -f base/static-components.h
