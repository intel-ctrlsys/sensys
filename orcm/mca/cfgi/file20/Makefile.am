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
        cfgi_file20.h \
        cfgi_file20.c \
        cfgi_file20_component.c

# Make the output library in this directory, and name it either
# mca_<project>_<type>_<name>.la (for DSO builds) or
# libmca_<project>_<type>_<name>.la (for static builds).

if MCA_BUILD_orcm_cfgi_file20_DSO
lib =
lib_sources =
component = mca_cfgi_file20.la
component_sources = $(sources)
else
lib = libmca_cfgi_file20.la
lib_sources = $(sources)
component =
ponent_sources =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_cfgi_file20_la_SOURCES = $(component_sources)
mca_cfgi_file20_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(lib)
libmca_cfgi_file20_la_SOURCES = $(lib_sources)
libmca_cfgi_file20_la_LDFLAGS = -module -avoid-version
