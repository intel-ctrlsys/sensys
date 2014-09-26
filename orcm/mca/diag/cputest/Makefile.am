#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        diag_cputest.h \
        diag_cputest_component.c \
        diag_cputest.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_diag_cputest_DSO
component_noinst =
component_install = mca_diag_cputest.la
else
component_noinst = libmca_diag_cputest.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_diag_cputest_la_SOURCES = $(sources)
mca_diag_cputest_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_diag_cputest_la_SOURCES =$(sources)
libmca_diag_cputest_la_LDFLAGS = -module -avoid-version
