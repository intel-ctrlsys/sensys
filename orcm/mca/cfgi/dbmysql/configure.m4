dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_cfgi_dbmysql_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_cfgi_dbmysql_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/cfgi/dbmysql/Makefile])

    AC_REQUIRE([OPAL_CHECK_MYSQL])

    AS_IF([test "$opal_check_mysql_happy" == "yes"],
          [orcm_cfgi_dbmysql_check_save_CPPFLAGS=$CPPFLAGS
           orcm_cfgi_dbmysql_check_save_LDFLAGS=$LDFLAGS
           orcm_cfgi_dbmysql_check_save_LIBS=$LIBS
           OPAL_CHECK_PACKAGE([cfgi_dbmysql],
                              [mysql.h],
                              [mysqlclient],
                              [mysql_library_init],
                              [],
                              [$orcm_check_mysql_dir],
                              [],
                              [$1],
                              [AC_MSG_WARN([MySQL database support requested])
                               AC_MSG_WARN([but required library or header not found])
                               AC_MSG_ERROR([Cannot continue])
                               $2])
           CPPFLAGS=$orcm_cfgi_dbmysql_check_save_CPPFLAGS
           LDFLAGS=$orcm_cfgi_dbmysql_check_save_LDFLAGS
           LIBS=$orcm_cfgi_dbmysql_check_save_LIBS],
          [$2])

    AC_SUBST(cfgi_dbmysql_CPPFLAGS)
    AC_SUBST(cfgi_dbmysql_LDFLAGS)
    AC_SUBST(cfgi_dbmysql_LIBS)
])dnl
