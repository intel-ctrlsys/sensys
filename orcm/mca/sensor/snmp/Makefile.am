#
# Copyright (c) 2015 Intel, Inc. All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

dist_orcmdata_DATA = help-orcm-sensor-snmp.txt

sources = \
    snmp_collector.h \
    snmp_collector.cpp \
    sensor_snmp.h \
    sensor_snmp.cpp \
    sensor_snmp_module.c \
    sensor_snmp_component.c \
    snmp.h \
    snmp.cpp \
    snmp_parser.h \
    snmp_parser.cpp \
    vardata.h \
    vardata.cpp

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_snmp_DSO
component_noinst =
component_install = mca_sensor_snmp.la
else
component_noinst = libmca_sensor_snmp.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)

mca_sensor_snmp_la_CPPFLAGS = $(sensor_snmp_CPPFLAGS) $(snmp_CPPFLAGS)
mca_sensor_snmp_la_SOURCES = $(sources)
mca_sensor_snmp_la_LDFLAGS = -module -avoid-version
mca_sensor_snmp_la_LIBADD = $(sensor_snmp_LIBS) $(snmp_LIBS) -lnetsnmp

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_snmp_la_SOURCES =$(sources)
libmca_sensor_snmp_la_CPPFLAGS = $(sensor_snmp_CPPFLAGS) $(snmp_CPPFLAGS)
libmca_sensor_snmp_la_LDFLAGS = -module -avoid-version $(sensor_snmp_LDFLAGS) $(snmp_LDFLAGS)
libmca_sensor_snmp_la_LIBADD = $(sensor_snmp_LIBS) $(snmp_LIBS) -lnetsnmp
