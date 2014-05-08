#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        sst_orun.h \
        sst_orun.c \
        sst_orun_component.c

# Make the output library in this directory, and name it either
# mca_<project>_<type>_<name>.la (for DSO builds) or
# libmca_<project>_<type>_<name>.la (for static builds).

if MCA_BUILD_orcm_sst_orun_DSO
lib =
lib_sources =
component = mca_sst_orun.la
component_sources = $(sources)
else
lib = libmca_sst_orun.la
lib_sources = $(sources)
component =
ponent_sources =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_sst_orun_la_SOURCES = $(component_sources)
mca_sst_orun_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(lib)
libmca_sst_orun_la_SOURCES = $(lib_sources)
libmca_sst_orun_la_LDFLAGS = -module -avoid-version
