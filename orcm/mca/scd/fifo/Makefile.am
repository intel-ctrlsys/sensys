#
# Copyright (c) 2014      Intel, Inc.  All rights reserved. 
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        scd_fifo.h \
        scd_fifo_component.c \
        scd_fifo.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_scd_fifo_DSO
component_noinst =
component_install = mca_scd_fifo.la
else
component_noinst = libmca_scd_fifo.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_scd_fifo_la_SOURCES = $(sources)
mca_scd_fifo_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_scd_fifo_la_SOURCES =$(sources)
libmca_scd_fifo_la_LDFLAGS = -module -avoid-version
