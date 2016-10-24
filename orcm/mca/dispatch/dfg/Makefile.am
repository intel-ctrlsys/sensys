#
# Copyright (c) 2015 Intel Corporation.  All rights reserved.
#
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        dispatch_dfg.h \
        dispatch_dfg.c \
        dispatch_dfg_component.c

# Make the output library in this directory, and name it either
# mca_<project>_<type>_<name>.la (for DSO builds) or
# libmca_<project>_<type>_<name>.la (for static builds).

if MCA_BUILD_orcm_dispatch_dfg_DSO
lib =
lib_sources =
component = mca_dispatch_dfg.la
component_sources = $(sources)
else
lib = libmca_dispatch_dfg.la
lib_sources = $(sources)
component =
ponent_sources =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_dispatch_dfg_la_SOURCES = $(component_sources)
mca_dispatch_dfg_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(lib)
libmca_dispatch_dfg_la_SOURCES = $(lib_sources)
libmca_dispatch_dfg_la_LDFLAGS = -module -avoid-version
