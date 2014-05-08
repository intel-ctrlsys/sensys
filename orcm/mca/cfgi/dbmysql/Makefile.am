#
# Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

sources = \
        cfgi_dbmysql.h \
        cfgi_dbmysql_component.c \
        cfgi_dbmysql.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_cfgi_dbmysql_DSO
component_noinst =
component_install = mca_cfgi_dbmysql.la
else
component_noinst = libmca_cfgi_dbmysql.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_cfgi_dbmysql_la_SOURCES = $(sources)
mca_cfgi_dbmysql_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_cfgi_dbmysql_la_SOURCES =$(sources)
libmca_cfgi_dbmysql_la_LDFLAGS = -module -avoid-version
