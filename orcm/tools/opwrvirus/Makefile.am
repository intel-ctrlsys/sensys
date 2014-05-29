#
# Copyright (c) 2014      Intel, Inc.  All rights reserved.
# $COPYRIGHT$
# 
# Additional copyrights may follow
# 
# $HEADER$
#

if OPAL_INSTALL_BINARIES

bin_PROGRAMS = opwrvirus

endif # OPAL_INSTALL_BINARIES

opwrvirus_SOURCES = opwrvirus.c
# the following empty orcmd_LDFLAGS is used 
#  so that the orcmd can be compiled statically 
#  by simply changing the value of this from 
#  nothing to -all-static in the Makefile.in
#  nice for systems that don't have all the shared 
#  libraries on the computes 
opwrvirus_LDFLAGS =
opwrvirus_LDADD = -lm
