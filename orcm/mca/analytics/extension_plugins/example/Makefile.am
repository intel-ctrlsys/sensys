#
# Copyright (c) 2016      Intel, Inc.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

example_sources = \
        extension_plugins/example/analytics_example.h \
        extension_plugins/example/analytics_example.cpp

component_noinst =
component_install = analytics_extension_example.la

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
analytics_extension_example_la_SOURCES = $(example_sources)
analytics_extension_example_la_LDFLAGS = -module -avoid-version