AC_INIT([telnIRC], [0.2], [mriron@undernet.org])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries
AC_CHECK_LIB([ncurses], [initscr], [], [AC_MSG_ERROR([ncurses library not found])])
AC_CHECK_LIB([pthread], [pthread_create], [], [AC_MSG_ERROR([pthread library not found])])

dnl Homebrew kegs are not on the default pkg-config search path. If `brew` is available,
dnl prepend the OpenSSL keg's pkgconfig directory so PKG_CHECK_MODULES finds openssl.pc
dnl without the user exporting PKG_CONFIG_PATH.
AC_MSG_CHECKING([whether to prepend Homebrew OpenSSL to PKG_CONFIG_PATH])
telnirc_hbrew_openssl_pc=""
if command -v brew >/dev/null 2>&1; then
  for telnirc_keg in openssl@3 openssl@1.1 openssl; do
    telnirc_bpfx=`brew --prefix "$telnirc_keg" 2>/dev/null` || continue
    if test -f "$telnirc_bpfx/lib/pkgconfig/openssl.pc"; then
      telnirc_hbrew_openssl_pc="$telnirc_bpfx/lib/pkgconfig"
      break
    fi
  done
fi
if test -n "$telnirc_hbrew_openssl_pc"; then
  PKG_CONFIG_PATH="$telnirc_hbrew_openssl_pc${PKG_CONFIG_PATH:+:}$PKG_CONFIG_PATH"
  export PKG_CONFIG_PATH
  AC_MSG_RESULT([yes ($telnirc_hbrew_openssl_pc)])
else
  AC_MSG_RESULT([no])
fi

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