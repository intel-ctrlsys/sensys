#
# Copyright (c) 2015      Intel, Inc.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
		analytics_cott.cpp \
		analytics_cott_component.c \
		analytics_cott.h \
		analyze_counter.cpp \
		analyze_counter.h \
		host_analyze_counters.cpp \
		host_analyze_counters.h

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_analytics_cott_DSO
component_noinst =
component_install = mca_analytics_cott.la
else
component_noinst = libmca_analytics_cott.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_analytics_cott_la_SOURCES = $(sources)
mca_analytics_cott_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_analytics_cott_la_SOURCES =$(sources)
libmca_analytics_cott_la_LDFLAGS = -module -avoid-version
