#
# Copyright (c) 2014      Intel, Inc. All rights reserved.
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        sensor_componentpower.c \
        sensor_componentpower.h \
        sensor_componentpower_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_componentpower_DSO
component_noinst =
component_install = mca_sensor_componentpower.la
else
component_noinst = libmca_sensor_componentpower.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_componentpower_la_SOURCES = $(sources)
mca_sensor_componentpower_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_componentpower_la_SOURCES =$(sources)
libmca_sensor_componentpower_la_LDFLAGS = -module -avoid-version

