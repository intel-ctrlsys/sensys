#
# Copyright (c) 2014-2015 Intel, Inc.  All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        evgen_ohpc1.h \
        evgen_ohpc1.c \
        evgen_ohpc1_component.c

# Make the output library in this directory, and name it either
# mca_<project>_<type>_<name>.la (for DSO builds) or
# libmca_<project>_<type>_<name>.la (for static builds).

if MCA_BUILD_orcm_evgen_ohpc1_DSO
lib =
lib_sources =
component = mca_evgen_ohpc1.la
component_sources = $(sources)
else
lib = libmca_evgen_ohpc1.la
lib_sources = $(sources)
component =
ponent_sources =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_evgen_ohpc1_la_SOURCES = $(component_sources)
mca_evgen_ohpc1_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(lib)
libmca_evgen_ohpc1_la_SOURCES = $(lib_sources)
libmca_evgen_ohpc1_la_LDFLAGS = -module -avoid-version
