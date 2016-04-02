# -*- makefile -*-
#
# Copyright (c) 2013-2016 Intel, Inc. All rights reserved.
## $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# This makefile.am does not stand on its own - it is included from orcm/Makefile.am

dist_orcmdata_DATA += runtime/help-orcm-runtime.txt

headers += \
        runtime/runtime.h \
        runtime/orcm_globals.h \
        runtime/orcm_info_support.h \
        runtime/orcm_cmd_server.h \
        runtime/led_control.h \
        runtime/led_control_interface.h \
        mca/sensor/ipmi/ipmi_collector.h \
        mca/sensor/ipmi/ipmi_parser.h \
        mca/sensor/ipmi/ipmi_parser_interface.h

liborcm_la_SOURCES += \
        runtime/orcm_finalize.c \
        runtime/orcm_globals.c \
        runtime/orcm_init.c \
        runtime/orcm_cmd_server.c \
        runtime/orcm_info_support.c \
        runtime/led_control.cpp \
        runtime/led_control_interface.cpp \
        mca/sensor/ipmi/ipmi_collector.cpp \
        mca/sensor/ipmi/ipmi_parser.cpp \
        mca/sensor/ipmi/ipmi_parser_interface.cpp

if HAVE_IPMI
liborcm_la_LDFLAGS += -lipmiutil -lcrypto
CPPFLAGS += -DHAVE_LED_CONTROL_SUPPORT
endif
