#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        analytics_window.h \
        analytics_window_component.c \
        analytics_window.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_analytics_window_DSO
component_noinst =
component_install = mca_analytics_window.la
else
component_noinst = libmca_analytics_window.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_analytics_window_la_SOURCES = $(sources)
mca_analytics_window_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_analytics_window_la_SOURCES =$(sources)
libmca_analytics_window_la_LDFLAGS = -module -avoid-version
