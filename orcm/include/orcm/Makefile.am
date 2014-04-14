# -*- makefile -*-
#
# Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

# This makefile.am does not stand on its own - it is included from orcm/Makefile.am

headers += \
	orcm/frameworks.h \
        orcm/constants.h \
        orcm/types.h

nodist_headers += \
	orcm/version.h

orcm/version.h: $(top_builddir)/config.status $(srcdir)/orcm/version.h.in
	cd $(top_builddir) && $(SHELL) ./config.status $(subdir)/$@
