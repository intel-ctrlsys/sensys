dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2012-2013 Los Alamos National Security, Inc. All rights reserved.
dnl Copyright (c) 2013-2014 Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_db_odbc_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_db_odbc_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/db/odbc/Makefile])

    AC_REQUIRE([OPAL_CHECK_ODBC])

    # do not build if support not requested
    AS_IF([test "$orcm_check_odbc_happy" == "yes"],
          [orcm_db_odbc_check_save_CPPFLAGS=$CPPFLAGS
           orcm_db_odbc_check_save_LDFLAGS=$LDFLAGS
           orcm_db_odbc_check_save_LIBS=$LIBS
           OPAL_CHECK_PACKAGE([db_odbc],
                              [libpq-fe.h],
                              [pq],
                              [PQconnectdb],
                              [],
                              [$orcm_odbc_incdir],
                              [],
                              [$1],
                              [AC_MSG_WARN([ODBC database support requested])
                               AC_MSG_WARN([but required library or header not found])
                               AC_MSG_ERROR([Cannot continue])
                               $2])
           CPPFLAGS=$orcm_db_odbc_check_save_CPPFLAGS
           LDFLAGS=$orcm_db_odbc_check_save_LDFLAGS
           LIBS=$orcm_db_odbc_check_save_LIBS],
          [$2])

    AC_SUBST(db_odbc_CPPFLAGS)
    AC_SUBST(db_odbc_LDFLAGS)
    AC_SUBST(db_odbc_LIBS)
])dnl
