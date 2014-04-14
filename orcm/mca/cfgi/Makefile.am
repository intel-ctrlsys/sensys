#
# Copyright (c) 2014      Intel, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# main library setup
noinst_LTLIBRARIES = libmca_cfgi.la
libmca_cfgi_la_SOURCES =

# local files
headers = cfgi.h \
        cfgi_types.h

libmca_cfgi_la_SOURCES += $(headers)

# Conditionally install the header files
if WANT_INSTALL_HEADERS
orcmdir = $(includedir)/openmpi/$(subdir)
nobase_orcm_HEADERS = $(headers)
endif

include base/Makefile.am

distclean-local:
	rm -f base/static-components.h
