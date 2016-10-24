#
# Copyright (c) 2014-2015 Intel Corporation.  All rights reserved.
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        scd_pmf.h \
        scd_pmf_component.c \
        scd_pmf.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_scd_pmf_DSO
component_noinst =
component_install = mca_scd_pmf.la
else
component_noinst = libmca_scd_pmf.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_scd_pmf_la_SOURCES = $(sources)
mca_scd_pmf_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_scd_pmf_la_SOURCES =$(sources)
libmca_scd_pmf_la_LDFLAGS = -module -avoid-version
