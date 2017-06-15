# -*- makefile -*-
#
# Copyright (c) 2016-2017  Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

# note - headers and nodist_headers will go in ${includedir}/openmpi,
# not ${includedir}/
headers = \
        dataContainer.hpp \
        UDExceptions.h \
        udsensors.h \
        baseFactory.h \
        dataContainerHelper.hpp \
        parsers/pugixml/pugixml.hpp \
        parsers/pugixml/pugiconfig.hpp \
        parsers/pugixml/pugixml.cpp \
        sensorconfig.hpp \
        sensorConfigNodeXml.hpp \
        xmlManager.hpp

lib_LTLIBRARIES = \
        libsensysplugins.la \
        libsensysplugins_helper.la

libsensysplugins_la_SOURCES = \
        dataContainer.cpp \
        baseFactory.cpp \
        sensorconfig.cpp \
        sensorConfigNodeXml.cpp \
        xmlManager.cpp

libsensysplugins_helper_la_SOURCES  = \
        dataContainerHelper.cpp

libsensysplugins_la_CXXFLAGS = -std=c++11
libsensysplugins_helper_la_CXXFLAGS = -std=c++11

if WANT_INSTALL_HEADERS
orcmdir = $(orcmincludedir)/orcm/common
nobase_dist_orcm_HEADERS = $(headers)
else
orcmdir = $(includedir)/orcm/common
nobase_dist_noinst_HEADERS = $(headers)
endif
