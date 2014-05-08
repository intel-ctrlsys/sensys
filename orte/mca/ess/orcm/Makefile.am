#
# Copyright (c) 2013-2014 Intel, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_ortedata_DATA = help-ess-orcm.txt

sources = \
        ess_orcm.h \
        ess_orcm_component.c \
        ess_orcm_module.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orte_ess_orcm_DSO
component_noinst =
component_install = mca_ess_orcm.la
else
component_noinst = libmca_ess_orcm.la
component_install =
endif

mcacomponentdir = $(ortelibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_ess_orcm_la_SOURCES = $(sources)
mca_ess_orcm_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_ess_orcm_la_SOURCES =$(sources)
libmca_ess_orcm_la_LDFLAGS = -module -avoid-version
