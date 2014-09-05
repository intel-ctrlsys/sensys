#
# Copyright (c) TBD
#
# Copyright (c) 2014      Intel, Inc. All rights reserved.
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        sensor_nodepower.c \
        sensor_nodepower.h \
        sensor_nodepower_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_nodepower_DSO
component_noinst =
component_install = mca_sensor_nodepower.la
else
component_noinst = libmca_sensor_nodepower.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_nodepower_la_SOURCES = $(sources)
mca_sensor_nodepower_la_LDFLAGS = -module -avoid-version
mca_sensor_nodepower_la_LIBADD = -lipmiutil -lpthread -lm

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_nodepower_la_SOURCES =$(sources)
libmca_sensor_nodepower_la_LDFLAGS = -module -avoid-version
libmca_sensor_nodepower_la_LIBADD = -lipmiutil -lpthread -lm

