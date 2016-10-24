#
# Copyright (c) 2014-2016      Intel Corporation.  All rights reserved.
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

headers += \
    base/analytics_private.h \
    base/base.h \
    base/analytics_factory.h \
    base/c_analytics_factory.h

sources =\
    base/analytics_base_frame.c \
    base/analytics_base_recv.c \
    base/analytics_base_select.c \
    base/analytics_base_stubs.c \
    base/analytics_base_db.c   \
    base/analytics_base_event.c \
    base/analytics_factory.cpp

libmca_analytics_la_SOURCES += $(sources)
libmca_analytics_la_LIBADD = $(top_builddir)/orcm/common/libsensysplugins.la
