#
# Copyright (c) 2013-2014 Intel, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_ortedata_DATA = help-routed-orcm.txt

sources = \
	routed_orcm.h \
	routed_orcm.c \
	routed_orcm_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orte_routed_orcm_DSO
component_noinst =
component_install = mca_routed_orcm.la
else
component_noinst = libmca_routed_orcm.la
component_install =
endif

mcacomponentdir = $(ortelibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_routed_orcm_la_SOURCES = $(sources)
mca_routed_orcm_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_routed_orcm_la_SOURCES =$(sources)
libmca_routed_orcm_la_LDFLAGS = -module -avoid-version
