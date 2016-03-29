#
# Copyright (c) 2013-2015 Intel, Inc. All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

dist_orcmdata_DATA = help-orcm-sensor-ipmi.txt

sources = \
        sensor_ipmi.c \
        sensor_ipmi.h \
        sensor_ipmi_decls.h \
        sensor_ipmi_component.c \
        ipmi_credentials.cpp \
        ipmi_credentials.h \
        ipmi_sel_collector.cpp \
        ipmi_sel_collector.h \
        persist_sel_record_id.cpp \
        persist_sel_record_id.h \
        sel_callback_defs.h \
        sel_record.cpp \
        sel_record.h \
        sensor_ipmi_sel.cpp \
        sensor_ipmi_sel.h \
        ipmi_collector.cpp \
        ipmi_collector.h \
        ipmi_parser.cpp \
        ipmi_parser.h

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
mca_sensor_ipmi_la_CPPFLAGS = $(sensor_ipmi_CPPFLAGS) $(ipmiutil_CPPFLAGS)
mca_sensor_ipmi_la_SOURCES = $(sources)
mca_sensor_ipmi_la_LDFLAGS = -module -avoid-version $(sensor_ipmi_LDFLAGS) $(ipmiutil_LDFLAGS)
mca_sensor_ipmi_la_LIBADD = $(sensor_ipmi_LIBS) $(ipmiutil_LIBS) -lm

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_ipmi_la_SOURCES =$(sources)
libmca_sensor_ipmi_la_CPPFLAGS = $(sensor_ipmi_CPPFLAGS) $(ipmiutil_CPPFLAGS)
libmca_sensor_ipmi_la_LDFLAGS = -module -avoid-version $(sensor_ipmi_LDFLAGS) $(ipmiutil_LDFLAGS)
libmca_sensor_ipmi_la_LIBADD = $(sensor_ipmi_LIBS) $(ipmiutil_LIBS) -lm
