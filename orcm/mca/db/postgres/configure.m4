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

# MCA_db_postgres_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orcm_db_postgres_CONFIG], [
    AC_CONFIG_FILES([orcm/mca/db/postgres/Makefile])

    AC_REQUIRE([OPAL_CHECK_POSTGRES])
    # This will set:
    # opal_db_postgres_found, opal_db_postgres_failed,
    # opal_db_postgres_CPPFLAGS, opal_db_postgres_LDFLAGS, and
    # opal_db_postgres_LIBS
    
    AS_IF([test "$opal_db_postgres_found" = "yes"],
          [$1],
          [$2
           AS_IF([test "$opal_db_postgres_failed" = "yes"],
                 [AC_MSG_ERROR([Cannot continue])])])
    
    AC_SUBST(opal_db_postgres_CPPFLAGS)
    AC_SUBST(opal_db_postgres_LDFLAGS)
    AC_SUBST(opal_db_postgres_LIBS)

])dnl
