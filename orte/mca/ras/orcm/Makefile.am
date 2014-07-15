#
# Copyright (c) 2014     Intel, Inc. All rights reserved. 
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

AM_CPPFLAGS = $(ras_orcm_CPPFLAGS)

dist_ortedata_DATA = help-ras-orcm.txt

sources = \
        ras_orcm.h \
        ras_orcm_component.c \
        ras_orcm_module.c


# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orte_ras_orcm_DSO
lib =
lib_sources = 
component = mca_ras_orcm.la
component_sources = $(sources)
else
lib = libmca_ras_orcm.la
lib_sources = $(sources)
component =
component_sources =
endif

mcacomponentdir = $(ortelibdir)
mcacomponent_LTLIBRARIES = $(component)
mca_ras_orcm_la_SOURCES = $(component_sources)
mca_ras_orcm_la_LDFLAGS = -module -avoid-version $(ras_orcm_LDFLAGS)
mca_ras_orcm_la_LIBADD = $(ras_orcm_LIBS)

noinst_LTLIBRARIES = $(lib)
libmca_ras_orcm_la_SOURCES = $(lib_sources)
libmca_ras_orcm_la_LDFLAGS = -module -avoid-version $(ras_orcm_LDFLAGS)
libmca_ras_orcm_la_LIBADD = $(ras_orcm_LIBS)
