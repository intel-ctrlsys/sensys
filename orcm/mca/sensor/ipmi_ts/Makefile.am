#
# Copyright (c) 2016     Intel Corporation. All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        sensor_ipmi_ts_component.c \
        sensor_ipmi_ts.h \
        sensor_ipmi_ts.cpp \
        ipmiSensorFactory.hpp \
        ipmiSensorFactory.cpp \
        ipmiSensorInterface.h \
        ipmiLibInterface.h \
        ipmiResponse.h \
        ipmiHAL.h \
        ipmiHAL.cpp \
        ipmiutilAgent.h \
        ipmiutilAgent.cpp \
        ipmiutilDFx.h \
        ipmiutilDFx.cpp \
        ipmiResponseFactory.hpp \
        ipmiResponseFactory.cpp \
        ipmiutilAgent_exceptions.h \
        ipmiutilAgent_exceptions.cpp \
        ipmi_ts_persist_sel_record_id.cpp \
        ipmi_ts_persist_sel_record_id.h \
        ipmi_ts_sel_callback_defs.h \
        ipmi_ts_sel_collector.cpp \
        ipmi_ts_sel_collector.h \
        ipmi_ts_sel_record.cpp \
        ipmi_ts_sel_record.h
 

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_sensor_ipmi_ts_DSO
component_noinst =
component_install = mca_sensor_ipmi_ts.la
else
component_noinst = libmca_sensor_ipmi_ts.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_sensor_ipmi_ts_la_CPPFLAGS = $(sensor_ipmi_ts_CPPFLAGS) $(ipmiutil_CPPFLAGS)
mca_sensor_ipmi_ts_la_CXXFLAGS = $(sensor_ipmi_ts_CXXFLAGS) $(ipmiutil_CXXFLAGS) -std=c++11
mca_sensor_ipmi_ts_la_SOURCES = $(sources)
mca_sensor_ipmi_ts_la_LDFLAGS = -module -avoid-version $(ipmiutil_LDFLAGS)
mca_sensor_ipmi_ts_la_LIBADD = $(top_builddir)/orcm/common/libsensysplugins.la \
                               $(top_builddir)/orcm/common/libsensysplugins_helper.la \
                               $(sensor_ipmi_ts_LIBS) $(ipmiutil_LIBS) -lm

noinst_LTLIBRARIES = $(component_noinst)
libmca_sensor_ipmi_ts_la_SOURCES =$(sources)
libmca_sensor_ipmi_ts_la_CPPFLAGS = $(sensor_ipmi_ts_CPPFLAGS) $(ipmiutil_CPPFLAGS)
libmca_sensor_ipmi_ts_la_CXXFLAGS = $(sensor_ipmi_ts_CXXFLAGS) $(ipmiutil_CXXFLAGS) -std=c++11
libmca_sensor_ipmi_ts_la_LDFLAGS = -module -avoid-version $(sensor_ipmi_LDFLAGS) $(ipmiutil_LDFLAGS)
libmca_sensor_ipmi_ts_la_LIBADD = $(top_builddir)/orcm/common/libsensysplugins.la \
                                  $(top_builddir)/orcm/common/libsensysplugins_helper.la \
                                  $(sensor_ipmi_LIBS) $(ipmiutil_LIBS) -lm

include ipmi_test_sensor/Makefile.am
include ipmi_sensor/Makefile.am
