#
# Copyright (c) 2009-2010 Cisco Systems, Inc.  All rights reserved.
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# main library setup
noinst_LTLIBRARIES = libmca_pwrmgmt.la
libmca_pwrmgmt_la_SOURCES =

# local files
headers = pwrmgmt.h \
          pwrmgmt_types.h

libmca_pwrmgmt_la_SOURCES += $(headers)

# Conditionally install the header files
if WANT_INSTALL_HEADERS
orcmdir = $(orcmincludedir)/$(subdir)
nobase_orcm_HEADERS = $(headers)
endif

include base/Makefile.am

distclean-local:
	rm -f base/static-components.h
