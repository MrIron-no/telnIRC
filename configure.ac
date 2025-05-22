AC_INIT([telnIRC], [0.2], [mriron@undernet.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries
AC_CHECK_LIB([ncurses], [initscr], [], [AC_MSG_ERROR([ncurses library not found])])
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread library not found])])

# Optionally check for OpenSSL if you use TLS
PKG_CHECK_MODULES([OPENSSL], [openssl], [have_openssl=yes], [have_openssl=no])

AM_CONDITIONAL([HAVE_OPENSSL], [test "x$have_openssl" = "xyes"])
if test "x$have_openssl" = "xyes"; then
  AC_DEFINE([HAVE_OPENSSL], [1], [Define if OpenSSL is available])
fi

# Checks for header files
AC_CHECK_HEADERS([ncurses.h])

AC_CONFIG_HEADERS([include/defs.h])
AC_CONFIG_FILES([Makefile])
AC_OUTPUT