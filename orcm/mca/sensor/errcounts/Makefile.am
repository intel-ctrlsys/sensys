#
# Copyright (c) 2015 Intel, Inc. All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

dist_orcmdata_DATA = help-orcm-sensor-errcounts.txt

sources = \
    edac_collector.h \
    edac_collector.cpp \
    sensor_errcounts.h \
    sensor_errcounts.cpp \
    sensor_errcounts_module.c \
    sensor_errcounts_component.c \
    errcounts.h \
    errcounts.cpp

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_coretemp_DSO
component_noinst =
component_install = mca_sensor_errcounts.la
else
component_noinst = libmca_sensor_errcounts.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_errcounts_la_SOURCES = $(sources)
mca_sensor_errcounts_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_errcounts_la_SOURCES =$(sources)
libmca_sensor_errcounts_la_LDFLAGS = -module -avoid-version

