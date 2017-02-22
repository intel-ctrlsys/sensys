#
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
    analytics_extension.cpp \
    analytics_extension_component.c \
    analytics_extension.h 

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_analytics_extension_DSO
component_noinst =
component_install = mca_analytics_extension.la
else
component_noinst = libmca_analytics_extension.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_analytics_extension_la_SOURCES = $(sources)
mca_analytics_extension_la_LDFLAGS = -module -avoid-version
mca_analytics_extension_la_CXXFLAGS = -std=c++11 

noinst_LTLIBRARIES = $(component_noinst)
libmca_analytics_extension_la_SOURCES =$(sources)
libmca_analytics_extension_la_LDFLAGS = -module -avoid-version
libmca_analytics_extension_la_CXXFLAGS = -std=c++11 
