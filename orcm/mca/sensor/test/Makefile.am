#
# Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_orcmdata_DATA = help-orcm-sensor-test.txt

sources = \
        sensor_test.c \
        sensor_test.h \
        sensor_test_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_test_DSO
component_noinst =
component_install = mca_sensor_test.la
else
component_noinst = libmca_sensor_test.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_test_la_SOURCES = $(sources)
mca_sensor_test_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_test_la_SOURCES =$(sources)
libmca_sensor_test_la_LDFLAGS = -module -avoid-version
