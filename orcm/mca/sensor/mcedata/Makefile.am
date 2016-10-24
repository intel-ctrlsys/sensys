#
# Copyright (c) 2013-2015 Intel Corporation. All rights reserved.
#
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_orcmdata_DATA = help-orcm-sensor-mcedata.txt

sources = \
        sensor_mcedata.c \
        sensor_mcedata.h \
        sensor_mcedata_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_mcedata_DSO
component_noinst =
component_install = mca_sensor_mcedata.la
else
component_noinst = libmca_sensor_mcedata.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_mcedata_la_SOURCES = $(sources)
mca_sensor_mcedata_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_mcedata_la_SOURCES =$(sources)
libmca_sensor_mcedata_la_LDFLAGS = -module -avoid-version
