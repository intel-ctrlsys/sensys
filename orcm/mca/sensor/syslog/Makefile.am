#
# Copyright (c) TBD
#
# Copyright (c) 2015 Intel Corporation. All rights reserved.
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        sensor_syslog.c \
        sensor_syslog.h \
        sensor_syslog_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_syslog_DSO
component_noinst =
component_install = mca_sensor_syslog.la
else
component_noinst = libmca_sensor_syslog.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_syslog_la_SOURCES = $(sources)
mca_sensor_syslog_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_syslog_la_SOURCES =$(sources)
libmca_sensor_syslog_la_LDFLAGS = -module -avoid-version
