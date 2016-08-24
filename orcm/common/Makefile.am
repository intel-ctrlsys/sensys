# -*- makefile -*-
#
# Copyright (c) 2016 Intel, Inc. All rights reserved.
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
        sensorInterface.h \
        baseFactory.h \
        dataContainerHelper.hpp

lib_LTLIBRARIES = \
        libsensysplugins.la \
        libsensysplugins_helper.la

libsensysplugins_la_SOURCES = \
        dataContainer.cpp \
        baseFactory.cpp

libsensysplugins_helper_la_SOURCES  = \
        dataContainerHelper.cpp

if WANT_INSTALL_HEADERS
orcmdir = $(orcmincludedir)/orcm/common
nobase_dist_orcm_HEADERS = $(headers)
else
orcmdir = $(includedir)/orcm/common
nobase_dist_noinst_HEADERS = $(headers)
endif
