#
# Copyright (c) 2016      Intel Corp. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        db_zeromq.h \
        db_zeromq_component.c \
        db_zeromq.cpp \
        Publisher.hpp \
        ZeroMQPublisher.hpp \
        ZeroMQPublisher.cpp


# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_db_zeromq_DSO
component_noinst =
component_install = mca_db_zeromq.la
else
component_noinst = libmca_db_zeromq.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES =  $(component_install)
mca_db_zeromq_la_CXXFLAGS = $(db_zeromq_CXXFLAGS) -std=c++11
mca_db_zeromq_la_CPPFLAGS = $(db_zeromq_CPPFLAGS)
mca_db_zeromq_la_SOURCES =  $(sources)
mca_db_zeromq_la_LDFLAGS =  -module -avoid-version $(db_zeromq_LDFLAGS)
mca_db_zeromq_la_LIBADD =   $(db_zeromq_LIBS) $(zeromq_LIBS)

noinst_LTLIBRARIES = $(component_noinst)
libmca_db_zeromq_la_CXXFLAGS = $(db_zeromq_CXXFLAGS) -std=c++11
libmca_db_zeromq_la_CPPFLAGS = $(db_zeromq_CPPFLAGS)
libmca_db_zeromq_la_SOURCES =  $(sources)
libmca_db_zeromq_la_LDFLAGS =  -module -avoid-version $(db_zeromq_LDFLAGS)
libmca_db_zeromq_la_LIBADD =   $(db_zeromq_LIBS) $(zeromq_LIBS)
