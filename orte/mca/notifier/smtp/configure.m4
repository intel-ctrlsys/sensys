dnl -*- shell-script -*-
dnl
dnl Copyright (c) 2015      Intel, Inc. All rights reserved.
dnl $COPYRIGHT$
dnl 
dnl Additional copyrights may follow
dnl 
dnl $HEADER$
dnl

# MCA_notifier_smtp_CONFIG([action-if-found], [action-if-not-found])
# -----------------------------------------------------------
AC_DEFUN([MCA_orte_notifier_smtp_CONFIG], [
    # Check if smtp support is available or not.
    AC_CONFIG_FILES([orte/mca/notifier/smtp/Makefile])
    AC_MSG_CHECKING([for SMTP notifier support])
    AC_REQUIRE([OPAL_CHECK_SMTP])

   AC_ARG_WITH([smtp],
                [AC_HELP_STRING([--with-smtp(=yes/no)],
                                [Build SMTP notifier support])],
                [AC_MSG_RESULT([with_smtp selected with parameter "$with_smtp"!])]
                [AS_IF([test "$smtp_check_happy" = "no" -a "$with_smtp" = "yes"],
                        AC_MSG_RESULT([SMTP Check failed and we need to skip building SMTP Sensor Component here])
                        AC_MSG_WARN([SMTP Libs not present])
                        AC_MSG_ERROR([SMTP Sensor requested but dependency not met])
                        $2)]
                [AS_IF([test "$smtp_check_happy" = "yes" -a "$with_smtp" = "yes"],
                        AC_MSG_RESULT([SMTP Check passed and SMTP notifier explicitly requested])
                        AC_MSG_RESULT([SMTP Sensor requested and dependency met and smtp notifier will be built])
                        $1)]
                [AS_IF([test "$smtp_check_happy" = "yes" -a "$with_smtp" = "no"],
                        AC_MSG_RESULT([SMTP Check passed and but SMTP notifier explicitly not requested])
                        AC_MSG_RESULT([SMTP Sensor not requested and smtp notifier will not be built])
                        $2)]
                [AS_IF([test "$smtp_check_happy" = "no" -a "$with_smtp" = "no"],
                        AC_MSG_RESULT([SMTP Check failed and SMTP notifier explicitly not requested])
                        AC_MSG_RESULT([SMTP Sensor not requested and dependency not met and smtp notifier will not be built])
                        $2)],
                [AC_MSG_RESULT([with_smtp not specified!])]
                [AS_IF([test "$smtp_check_happy" = "no"],
                        AC_MSG_RESULT([SMTP Check failed and SMTP notifier not requested])
                        AC_MSG_RESULT([SMTP Sensor not requested and dependency not met and smtp notifier will not be built])
                        $2)]
                [AS_IF([test "$smtp_check_happy" = "yes"],
                        AC_MSG_RESULT([SMTP Check passed and SMTP notifier not requested])
                        AC_MSG_RESULT([SMTP Sensor not explicitly requested but dependency was met and smtp notifier will be built])
                        $1)])
    
])dnl
