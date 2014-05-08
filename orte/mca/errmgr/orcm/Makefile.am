#
# Copyright (c) 2013-2014 Intel, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        errmgr_orcm.h \
        errmgr_orcm_component.c \
        errmgr_orcm.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orte_errmgr_orcm_DSO
component_noinst =
component_install = mca_errmgr_orcm.la
else
component_noinst = libmca_errmgr_orcm.la
component_install =
endif

mcacomponentdir = $(ortelibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_errmgr_orcm_la_SOURCES = $(sources)
mca_errmgr_orcm_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_errmgr_orcm_la_SOURCES =$(sources)
libmca_errmgr_orcm_la_LDFLAGS = -module -avoid-version
