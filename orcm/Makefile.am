# -*- makefile -*-
#
# Copyright (c) 2013      Intel, Inc. All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

AM_CFLAGS = $(ORCM_CFLAGS)

# Note that the ordering of "." in SUBDIRS is important
SUBDIRS = \
        etc \
	include \
	$(EXT_orcm_FRAMEWORKS_SUBDIRS) \
	$(EXT_orcm_FRAMEWORK_COMPONENT_STATIC_SUBDIRS) \
	$(MCA_orcm_FRAMEWORKS_SUBDIRS) \
	$(MCA_orcm_FRAMEWORK_COMPONENT_STATIC_SUBDIRS) \
	. \
	$(MCA_orcm_FRAMEWORK_COMPONENT_DSO_SUBDIRS) 
    
DIST_SUBDIRS = \
        etc \
	include \
	$(EXT_orcm_FRAMEWORKS_SUBDIRS) \
	$(EXT_orcm_FRAMEWORK_COMPONENT_ALL_SUBDIRS) \
	$(MCA_orcm_FRAMEWORKS_SUBDIRS) \
	$(MCA_orcm_FRAMEWORK_COMPONENT_ALL_SUBDIRS) 

#Build The main ORCM library 
lib_LTLIBRARIES = liborcm.la
liborcm_la_SOURCES =
liborcm_la_LIBADD = \
	$(MCA_orcm_FRAMEWORK_LIBS) \
	$(top_opal_builddir)/orte/libopen-rte.la
liborcm_la_DEPENDENCIES = $(liborcm_la_LIBADD)
liborcm_la_LDFLAGS = \
        -version-info $(liborcm_so_version) \
        $(ORCM_LIBORCM_EXTRA_LDFLAGS)

# included subdirectory Makefile.am's and appended-to variables
headers =
noinst_LTLIBRARIES =
include_HEADERS =
nobase_orcm_HEADERS = 
dist_ompidata_DATA = 
liborcm_la_SOURCES += $(headers)
nodist_man_MANS =

# Conditionally install the header files

if WANT_INSTALL_HEADERS
orcmdir = $(ompiincludedir)/$(subdir)
nobase_orcm_HEADERS += $(headers)
else
orcmdir = $(includedir)
endif

include runtime/Makefile.am
include tools/Makefile.am
include util/Makefile.am

# Ensure that the man page directory exists before we try to make man
# page files (because orcm/orcm/man/man3 has no config.status-generated
# Makefile)
dir_stamp = $(top_builddir)/$(subdir)/orcm/man/man3/.dir-stamp

# Also ensure that the man pages are rebuilt if the opal_config.h file
# changes (e.g., configure was run again, meaning that the release
# date or version may have changed)
$(nodist_man_MANS): $(dir_stamp) $(top_builddir)/opal/include/opal_config.h

$(dir_stamp):
	$(mkdir_p) `dirname $@`
	touch "$@"

# Remove the generated man pages
distclean-local:
	rm -f $(nodist_man_MANS) $(dir_stamp)
