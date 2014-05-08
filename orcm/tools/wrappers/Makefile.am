#
# Copyright (c) 2004-2005 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2005 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2006-2012 Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2013      Intel, Inc. All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

#man_pages = orcmcc.1

if !OPAL_WANT_SCRIPT_WRAPPER_COMPILERS
if OPAL_INSTALL_BINARIES

nodist_orcmdata_DATA = \
	orcmcc-wrapper-data.txt

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = orcm.pc

install-exec-hook:
	test -z "$(bindir)" || $(MKDIR_P) "$(DESTDIR)$(bindir)"
	(cd $(DESTDIR)$(bindir); rm -f orcmcc$(EXEEXT); $(LN_S) opal_wrapper orcmcc)

uninstall-local:
	rm -f $(DESTDIR)$(bindir)/orcmcc$(EXEEXT)

endif # OPAL_INSTALL_BINARIES

endif # OPAL_WANT_SCRIPT_WRAPPER_COMPILERS
