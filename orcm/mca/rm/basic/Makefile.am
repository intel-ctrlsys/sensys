#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        rm_basic.h \
        rm_basic_component.c \
        rm_basic.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_rm_basic_DSO
component_noinst =
component_install = mca_rm_basic.la
else
component_noinst = libmca_rm_basic.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_rm_basic_la_SOURCES = $(sources)
mca_rm_basic_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_rm_basic_la_SOURCES =$(sources)
libmca_rm_basic_la_LDFLAGS = -module -avoid-version
