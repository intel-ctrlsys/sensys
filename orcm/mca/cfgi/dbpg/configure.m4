dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2014      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_cfgi_dbpg_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_cfgi_dbpg_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/cfgi/dbpg/Makefile])

    AC_REQUIRE([OPAL_CHECK_POSTGRES])

    AS_IF([test "$opal_check_postgres_happy" == "yes"],
          [orcm_cfgi_dbpg_check_save_CPPFLAGS=$CPPFLAGS
           orcm_cfgi_dbpg_check_save_LDFLAGS=$LDFLAGS
           orcm_cfgi_dbpg_check_save_LIBS=$LIBS
           OPAL_CHECK_PACKAGE([cfgi_dbpg],
                              [libpq-fe.h],
                              [pq],
                              [PQconnectdb],
                              [],
                              [$opal_postgres_incdir],
                              [],
                              [$1],
                              [AC_MSG_WARN([Postgres database support requested])
                               AC_MSG_WARN([but required library or header not found])
                               AC_MSG_ERROR([Cannot continue])
                               $2])
           CPPFLAGS=$orcm_cfgi_dbpg_check_save_CPPFLAGS
           LDFLAGS=$orcm_cfgi_dbpg_check_save_LDFLAGS
           LIBS=$orcm_cfgi_dbpg_check_save_LIBS],
          [$2])


    AC_SUBST(cfgi_dbpg_CPPFLAGS)
    AC_SUBST(cfgi_dbpg_LDFLAGS)
    AC_SUBST(cfgi_dbpg_LIBS)
])dnl
