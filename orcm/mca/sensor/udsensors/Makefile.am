#
# Copyright (c) 2016     Intel, Inc. All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        sensor_udsensors_component.c \
        sensor_udsensors.h \
        sensor_udsensors.cpp \
        sensorFactory.h \
        sensorFactory.cpp

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_udsensors_DSO
component_noinst =
component_install = mca_sensor_udsensors.la
else
component_noinst = libmca_sensor_udsensors.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_udsensors_la_SOURCES = $(sources)
mca_sensor_udsensors_la_LDFLAGS = -module -avoid-version
mca_sensor_udsensors_la_LIBADD = $(top_builddir)/orcm/common/libsensysplugins.la

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_udsensors_la_SOURCES =$(sources)
libmca_sensor_udsensors_la_LDFLAGS = -module -avoid-version
libmca_sensor_udsensors_la_LIBADD = $(top_builddir)/orcm/common/libsensysplugins.la
