#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_orcmdata_DATA = help-sst-orcmctrld.txt

sources = \
        sst_orcmctrld.h \
        sst_orcmctrld.c \
        sst_orcmctrld_component.c

# Make the output library in this directory, and name it either
# mca_<project>_<type>_<name>.la (for DSO builds) or
# libmca_<project>_<type>_<name>.la (for static builds).

if MCA_BUILD_orcm_sst_orcmctrld_DSO
lib =
lib_sources =
component = mca_sst_orcmctrld.la
component_sources = $(sources)
else
lib = libmca_sst_orcmctrld.la
lib_sources = $(sources)
component =
ponent_sources =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_sst_orcmctrld_la_SOURCES = $(component_sources)
mca_sst_orcmctrld_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(lib)
libmca_sst_orcmctrld_la_SOURCES = $(lib_sources)
libmca_sst_orcmctrld_la_LDFLAGS = -module -avoid-version
