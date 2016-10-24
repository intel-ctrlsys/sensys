# -*- shell-script -*-
#
# Copyright (c) 2014      Intel Corporation. All rights reserved.
# Copyright (c) 2016      Intel Corporation. All rights reserved.
# $COPYRIGHT$
#
# Additional copyrights may follow
#
# $HEADER$
#
# --------------------------------------------------------


# OPAL_CHECK_NETSNMP()
# -----------------------------------------------------------

AC_DEFUN([OPAL_CHECK_NETSNMP], [
    AC_MSG_CHECKING([for NET-SNMP support])

    snmp_prefix_dir="/usr"
    snmp_check_happy=no
    snmp_requested=no

    AC_ARG_WITH([snmp],
                [AC_HELP_STRING([--with-snmp(=DIR)],
                                [Build SNMP support, optionally adding DIR to the search path])],
                [# with_s=yes|no|path
                  AS_IF([test "$with_snmp" = "no"],
                        [# Support explicitly not requested
                         AC_MSG_RESULT([SNMP support not requested])
                        ],
                        [# Support explicitly requested (with_snmp=yes|path)
                           AC_MSG_RESULT([SNMP support explicitly requested])
                           snmp_requested=yes
                           AS_IF([test "$with_snmp" != "yes"],
                                 [# SNMP directory provided
                                    snmp_prefix_dir=$with_snmp
                                    AC_MSG_RESULT([with_snmp Directory is $snmp_prefix_dir])
                                 ])
                        ])
                ],
                [# Support not explicitly requested, try to build if possible
                     AC_MSG_RESULT([SNMP support NOT explicitly requested, checking library])
                     # Perform check
                ])

    AS_IF([test -f "$snmp_prefix_dir/include/net-snmp/net-snmp-includes.h"],
          [# Library found
            AC_MSG_RESULT([SNMP library found])
            snmp_check_happy=yes
          ],
          [# Library not found
            AC_MSG_RESULT([SNMP library not found])
            snmp_check_happy=no
          ])

    AS_IF([test "$snmp_requested" == "yes" && test "$snmp_check_happy" == "no"],
          [AC_MSG_ERROR([SNMP support requested but library has not been found])])

    AC_SUBST(snmp_CPPFLAGS)
    AC_SUBST(snmp_LDFLAGS)
    AC_SUBST(snmp_LIBS)

])dnl
