#
# Copyright (c) 2009-2011 Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2015      Intel, Inc. All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        sensor_evinj.c \
        sensor_evinj.h \
        sensor_evinj_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_evinj_DSO
component_noinst =
component_install = mca_sensor_evinj.la
else
component_noinst = libmca_sensor_evinj.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_evinj_la_SOURCES = $(sources)
mca_sensor_evinj_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_evinj_la_SOURCES =$(sources)
libmca_sensor_evinj_la_LDFLAGS = -module -avoid-version
