#
# Copyright (c) 2004-2010 The Trustees of Indiana University and Indiana
#                         University Research and Technology
#                         Corporation.  All rights reserved.
# Copyright (c) 2004-2005 The University of Tennessee and The University
#                         of Tennessee Research Foundation.  All rights
#                         reserved.
# Copyright (c) 2004-2009 High Performance Computing Center Stuttgart, 
#                         University of Stuttgart.  All rights reserved.
# Copyright (c) 2004-2005 The Regents of the University of California.
#                         All rights reserved.
# Copyright (c) 2009      Cisco Systems, Inc.  All rights reserved.
# Copyright (c) 2010      IBM Corporation.  All rights reserved.
# Copyright (c) 2010-2011 Oak Ridge National Labs.  All rights reserved.
# Copyright (c) 2013      Los Alamos National Security, Inc.  All rights reserved.
# Copyright (c) 2013-2014 Intel Corporation.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

amca_paramdir = $(AMCA_PARAM_SETS_DIR)
dist_amca_param_DATA = amca-param-sets/example.conf

if WANT_FT_CR
dist_amca_param_DATA += \
	amca-param-sets/ft-enable-cr \
	amca-param-sets/ft-enable-cr-recovery
endif # WANT_FT_CR

EXTRA_DIST = \
        completion/mpirun.sh \
        completion/mpirun.zsh \
	dist/make_dist_tarball \
	dist/linux/openmpi.spec \
	dist/mofed/compile_debian_mlnx_example.in \
	dist/mofed/debian \
	dist/macosx-pkg/buildpackage.sh \
	dist/macosx-pkg/ReadMe.rtf \
	platform/intel/hillsboro/orcm-linux \
	platform/intel/hillsboro/orcm-linux.conf \
	platform/intel/hillsboro/orcm-linux.xml

dist_opaldata_DATA = openmpi-valgrind.supp
