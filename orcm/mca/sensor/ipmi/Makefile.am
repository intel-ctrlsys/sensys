#
# Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

dist_orcmdata_DATA = help-orcm-sensor-ipmi.txt

sources = \
        ipmiutil-2.9.3/util/ievents.c \
        ipmiutil-2.9.3/util/ievents.h \
        ipmiutil-2.9.3/util/isensor.c \
        ipmiutil-2.9.3/util/isensor.h \
        sensor_ipmi.c \
        sensor_ipmi.h \
        sensor_ipmi_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_ipmi_DSO
component_noinst =
component_install = mca_sensor_ipmi.la
else
component_noinst = libmca_sensor_ipmi.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_ipmi_la_CPPFLAGS = $(sensor_ipmi_CPPFLAGS)
mca_sensor_ipmi_la_SOURCES = $(sources)
mca_sensor_ipmi_la_LDFLAGS = -module -avoid-version $(sensor_ipmi_LDFLAGS)
mca_sensor_ipmi_la_LIBADD = $(sensor_ipmi_LIBS) -lm

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_ipmi_la_SOURCES =$(sources)
libmca_sensor_ipmi_la_CPPFLAGS = $(sensor_ipmi_CPPFLAGS)
libmca_sensor_ipmi_la_LDFLAGS = -module -avoid-version $(sensor_ipmi_LDFLAGS)
libmca_sensor_ipmi_la_LIBADD = $(sensor_ipmi_LIBS) -lm
