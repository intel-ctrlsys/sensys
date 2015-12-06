#
# Copyright (c) 2015 Intel, Inc.  All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        evgen_saeg.h \
        evgen_saeg.c \
        evgen_saeg_component.c

# Make the output library in this directory, and name it either
# mca_<project>_<type>_<name>.la (for DSO builds) or
# libmca_<project>_<type>_<name>.la (for static builds).

if MCA_BUILD_orcm_evgen_saeg_DSO
lib =
lib_sources =
component = mca_evgen_saeg.la
component_sources = $(sources)
else
lib = libmca_evgen_saeg.la
lib_sources = $(sources)
component =
ponent_sources =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_evgen_saeg_la_SOURCES = $(component_sources)
mca_evgen_saeg_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(lib)
libmca_evgen_saeg_la_SOURCES = $(lib_sources)
libmca_evgen_saeg_la_LDFLAGS = -module -avoid-version
