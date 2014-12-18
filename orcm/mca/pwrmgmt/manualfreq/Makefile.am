#
# Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
#
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

#dist_orcmdata_DATA = help-orcm-pwrmgmt-manualfreq.txt

sources = \
        pwrmgmt_manualfreq.c \
        pwrmgmt_manualfreq.h \
        pwrmgmt_manualfreq_component.c

# Make the output library in this directory, and name it either
# mca_<type>_<name>.la (for DSO builds) or libmca_<type>_<name>.la
# (for static builds).

if MCA_BUILD_orcm_pwrmgmt_manualfreq_DSO
component_noinst =
component_install = mca_pwrmgmt_manualfreq.la
else
component_noinst = libmca_pwrmgmt_manualfreq.la
component_install =
endif

mcacomponentdir = $(orcmlibdir)
mcacomponent_LTLIBRARIES = $(component_install)
mca_pwrmgmt_manualfreq_la_SOURCES = $(sources)
mca_pwrmgmt_manualfreq_la_LDFLAGS = -module -avoid-version

noinst_LTLIBRARIES = $(component_noinst)
libmca_pwrmgmt_manualfreq_la_SOURCES =$(sources)
libmca_pwrmgmt_manualfreq_la_LDFLAGS = -module -avoid-version
