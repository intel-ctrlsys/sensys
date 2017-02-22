#
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

average_sources = \
        extension_plugins/average/analytics_average.h \
        extension_plugins/average/analytics_average.cpp

component_noinst =
component_install = analytics_extension_average.la

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
analytics_extension_average_la_SOURCES = $(average_sources)
analytics_extension_average_la_LDFLAGS = -module -avoid-version
analytics_extension_average_la_CXXFLAGS = -std=c++11
