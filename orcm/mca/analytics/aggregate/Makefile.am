#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        analytics_aggregate.h \
        analytics_aggregate_component.c \
        analytics_aggregate.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_analytics_aggregate_DSO
component_noinst =
component_install = mca_analytics_aggregate.la
else
component_noinst = libmca_analytics_aggregate.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_analytics_aggregate_la_SOURCES = $(sources)
mca_analytics_aggregate_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_analytics_aggregate_la_SOURCES =$(sources)
libmca_analytics_aggregate_la_LDFLAGS = -module -avoid-version
