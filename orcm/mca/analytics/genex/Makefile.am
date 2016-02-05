#
# Copyright (c) 2015-2016 Intel, Inc.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        analytics_genex.h \
        analytics_genex_component.c \
        analytics_genex.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_analytics_filter_DSO
component_noinst =
component_install = mca_analytics_genex.la
else
component_noinst = libmca_analytics_genex.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_analytics_genex_la_SOURCES = $(sources)
mca_analytics_genex_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_analytics_genex_la_SOURCES =$(sources)
libmca_analytics_genex_la_LDFLAGS = -module -avoid-version
