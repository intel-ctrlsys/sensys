#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        pvsn_ww.h \
        pvsn_ww_component.c \
        pvsn_ww.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_pvsn_wwulf_DSO
component_noinst =
component_install = mca_pvsn_wwulf.la
else
component_noinst = libmca_pvsn_wwulf.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_pvsn_wwulf_la_SOURCES = $(sources)
mca_pvsn_wwulf_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_pvsn_wwulf_la_SOURCES =$(sources)
libmca_pvsn_wwulf_la_LDFLAGS = -module -avoid-version
