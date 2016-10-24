#
# Copyright (c) 2016 Intel Corporation. All rights reserved.
#
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

sources = \
        parser_pugi_component.c \
        parser_pugi.h \
        parser_pugi.cpp \
        pugi_impl.h \
        pugi_impl.cpp \
        pugixml/pugiconfig.hpp \
        pugixml/pugixml.cpp \
        pugixml/pugixml.hpp

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_parser_pugi_DSO
component_noinst =
component_install = mca_parser_pugi.la
else
component_noinst = libmca_parser_pugi.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_parser_pugi_la_CPPFLAGS = $(parser_pugi_CPPFLAGS) $(pugiutil_CPPFLAGS)
mca_parser_pugi_la_SOURCES = $(sources)
mca_parser_pugi_la_LDFLAGS = -module -avoid-version $(parser_pugi_LDFLAGS) $(pugiutil_LDFLAGS)

noinst_LTLIBRARIES = $(component_noinst)
libmca_parser_pugi_la_SOURCES =$(sources)
libmca_parser_pugi_la_CPPFLAGS = $(parser_pugi_CPPFLAGS) $(pugiutil_CPPFLAGS)
libmca_parser_pugi_la_LDFLAGS = -module -avoid-version $(parser_pugi_LDFLAGS) $(pugiutil_LDFLAGS)
