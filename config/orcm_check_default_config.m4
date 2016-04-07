# -*- shell-script -*-
#
# Copyright (c) 2016  Intel, Inc. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#

dnl ORCM_CHECK_DEFAULT_CONFIG processes these options:
dnl
dnl  --with-default-config=FILE
dnl     path to directory containing orcm default configuration file.
dnl
dnl If the file is found, we set:
dnl
dnl  @ORCM_DEFAULT_CONFIG_FILE@            variable in Makefile.am
dnl  @ORCM_DEFAULT_CONFIG_FILE_FOUND@      variable in Makefile.am
dnl
dnl

AC_DEFUN([ORCM_CHECK_DEFAULT_CONFIG], [

  orcm_default_config=default
  orcm_default_config_found=no

  AC_MSG_CHECKING([ORCM default configuration file.])

  AC_ARG_WITH([default-config],

    [AS_HELP_STRING([--with-default-config=yes|FILE],
      [ FILE - path to file containing default configuration for ORCM: IPMI,
        SNMP, Workflows, Logical Group, etc.])],

    [ if test "$with_default_config" != "" ; then
        if test "$with_default_config" != "yes" ; then
          if test -f "$with_default_config" ; then
            orcm_default_config=$with_default_config
          else
            AC_MSG_WARN([cannot find ORCM default configuration file at $with_default_config
                      Using default location.])
          fi
        fi
      fi
    ]

    [])

   if test "$orcm_default_config" = "default" ; then
     orcm_default_config="${srcdir}/orcm/etc/orcm-default-config.xml"
   elif test "`echo $orcm_default_config | cut -c1`" != "/" ; then
     orcm_default_config_base="`dirname $orcm_default_config`"
     orcm_default_config_name="`basename $orcm_default_config`"
     current_dir="`pwd`"
     cd "$orcm_default_config_base"
     orcm_default_config_dir="`pwd`"
     orcm_default_config="$orcm_default_config_dir/$orcm_default_config_name"
     cd "$current_dir"
   fi
   if test -f "$orcm_default_config" ; then
      orcm_default_config_found=yes
   fi
   AC_MSG_RESULT([$orcm_default_config_found])
   AC_MSG_NOTICE([ORCM default config file used: $orcm_default_config])
   AC_SUBST([ORCM_DEFAULT_CONFIG_FILE], [$orcm_default_config])
   AC_SUBST([ORCM_DEFAULT_CONFIG_FILE_FOUND], [$orcm_default_config_found])

 ])
