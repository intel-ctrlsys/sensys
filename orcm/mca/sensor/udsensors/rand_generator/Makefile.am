#
# Copyright (c) 2016      Intel, Inc.  All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

rand_generator_sources = \
        rand_generator/rand_generator.hpp \
        rand_generator/rand_generator.cpp

if WITH_RAND_GENERATOR
component_install += libudplugin_rand_generator.la
endif

libudplugin_rand_generator_la_SOURCES = $(rand_generator_sources)
libudplugin_rand_generator_la_LDFLAGS = -module -avoid-version
libudplugin_rand_generator_la_LIBADD = $(top_builddir)/orcm/common/libsensysplugins.la
