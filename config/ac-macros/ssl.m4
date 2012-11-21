dnl ===========================================================================
dnl Support for SSL
dnl ===========================================================================
dnl
dnl

dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_USE_BUNDLED_YASSL
dnl
dnl SYNOPSIS
dnl   MYSQL_USE_BUNDLED_YASSL()
dnl
dnl DESCRIPTION
dnl  Add defines so yassl is built and linked with
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_USE_BUNDLED_YASSL], [

  with_bundled_yassl="yes"

  yassl_dir="yassl"
  AC_SUBST([yassl_dir])

  yassl_libs="\$(top_builddir)/extra/yassl/src/libyassl.la \
              \$(top_builddir)/extra/yassl/taocrypt/src/libtaocrypt.la"
  AC_SUBST(yassl_libs)

  AC_DEFINE([HAVE_OPENSSL], [1], [Defined by configure. Using yaSSL for SSL.])
  AC_DEFINE([HAVE_YASSL], [1], [Defined by configure. Using yaSSL for SSL.])

  # System specific checks
  yassl_integer_extra_cxxflags=""
  case $host_cpu--$CXX_VERSION in
      sparc*--*Sun*C++*5.6*)
	# Disable inlining when compiling taocrypt/src/
	yassl_taocrypt_extra_cxxflags="+d"
        AC_MSG_NOTICE([disabling inlining for yassl/taocrypt/src/])
        ;;
  esac
  AC_SUBST([yassl_taocrypt_extra_cxxflags])

  # Thread safe check
  yassl_thread_cxxflags=""
  yassl_thread_safe=""
  if test "$with_server" != "no" -o "$THREAD_SAFE_CLIENT" != "no"; then
    yassl_thread_cxxflags="-DMULTI_THREADED"
    yassl_thread_safe="(thread-safe)"
  fi
  AC_SUBST([yassl_thread_cxxflags])

  # Link extra/yassl/include/openssl subdir to include/
  yassl_h_ln_cmd="\$(LN) -s \$(top_srcdir)/extra/yassl/include/openssl openssl"
  AC_SUBST(yassl_h_ln_cmd)

  AC_MSG_RESULT([using bundled yaSSL $yassl_thread_safe])
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_SSL_DIR
dnl
dnl SYNOPSIS
dnl   MYSQL_CHECK_SSL_DIR(includes, libs)
dnl
dnl DESCRIPTION
dnl  Auxiliary macro to check for ssl at given path
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_CHECK_SSL_DIR], [
ssl_incs="$1"
ssl_libs="$2"
save_CPPFLAGS="$CPPFLAGS"
save_LIBS="$LIBS"
CPPFLAGS="$ssl_incs $CPPFLAGS"
LIBS="$LIBS $ssl_libs"
AC_TRY_LINK([#include <openssl/ssl.h>],
    [return SSL_library_init();],
    [mysql_ssl_found="yes"],
    [mysql_ssl_found="no"])
CPPFLAGS="$save_CPPFLAGS"
LIBS="$save_LIBS"
])


dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_FIND_OPENSSL
dnl
dnl SYNOPSIS
dnl   MYSQL_FIND_OPENSSL(location)
dnl
dnl DESCRIPTION
dnl  Search the location for OpenSSL support
dnl
dnl ---------------------------------------------------------------------------
AC_DEFUN([MYSQL_FIND_OPENSSL], [
  location="$1"

  #
  # Set include paths
  #
  openssl_include="$location/include"
  openssl_includes=""

  # Don't set ssl_includes to /usr/include as this gives us a lot of
  # compiler warnings when using gcc 3.x
  if test "$openssl_include" != "/usr/include"
  then
    openssl_includes="-I$openssl_include"
  fi

  #
  # Try to link with openSSL libs in <location>
  #
  openssl_libs="-L$location/lib/ -lssl -lcrypto"
  MYSQL_CHECK_SSL_DIR([$openssl_includes], [$openssl_libs])

  if test "$mysql_ssl_found" == "no"
  then
    #
    # BUG 764: Compile failure with OpenSSL on Red Hat Linux (krb5.h missing)
    # Try to link with include paths to kerberos set
    #
    openssl_includes="$openssl_includes -I/usr/kerberos/include"
    MYSQL_CHECK_SSL_DIR([$openssl_includes], [$openssl_libs])
  fi

  if test "$mysql_ssl_found" == "no"
  then
    AC_MSG_ERROR([Could not link with SSL libs at $location])
  fi

  # openssl-devel-0.9.6 requires dlopen() and we can't link staticly
  # on many platforms (We should actually test this here, but it's quite
  # hard to do as we are doing libtool for linking.)
  case "$CLIENT_EXTRA_LDFLAGS $MYSQLD_EXTRA_LDFLAGS" in
    *-all-static*)
    AC_MSG_ERROR([You can't use the --all-static link option when using openssl.])
    ;;
  esac

  AC_SUBST(openssl_includes)
  AC_SUBST(openssl_libs)

  NON_THREADED_CLIENT_LIBS="$NON_THREADED_CLIENT_LIBS $openssl_libs"

  AC_DEFINE([HAVE_OPENSSL], [1], [OpenSSL])
  AC_MSG_RESULT([using openSSL from $location])
])



dnl ------------------------------------------------------------------------
dnl Macro: MYSQL_CHECK_SSL
dnl
dnl SYNOPSIS
dnl   MYSQL_CHECK_SSL
dnl
dnl Provides the following configure options:
dnl --with-ssl=DIR
dnl Possible DIR values are:
dnl - no - the macro will disable use of ssl
dnl - bundled, empty or not specified - means use ssl lib
dnl   bundled along with MySQL sources
dnl - ssl location prefix - given location prefix, the macro expects
dnl   to find the header files in $prefix/include/, and libraries in
dnl   $prefix/lib. If headers or libraries weren't found at $prefix, the
dnl   macro bails out with error.
dnl
dnl ------------------------------------------------------------------------
AC_DEFUN([MYSQL_CHECK_SSL], [

  AC_CONFIG_FILES(extra/yassl/Makefile dnl
    extra/yassl/taocrypt/Makefile dnl
    extra/yassl/taocrypt/benchmark/Makefile dnl
    extra/yassl/taocrypt/src/Makefile dnl
    extra/yassl/taocrypt/test/Makefile dnl
    extra/yassl/src/Makefile dnl
    extra/yassl/testsuite/Makefile)

AC_MSG_CHECKING(for SSL)
  AC_ARG_WITH([ssl],
              [  --with-ssl[=DIR]    Include SSL support],
              [mysql_ssl_dir="$withval"],
              [mysql_ssl_dir=no])

  if test "$with_yassl"
  then
    AC_MSG_ERROR([The flag --with-yassl is deprecated, use --with-ssl])
  fi

  if test "$with_openssl"
  then
    AC_MSG_ERROR([The flag --with-openssl is deprecated, use --with-ssl])
  fi

  case "$mysql_ssl_dir" in
    "no")
      #
      # Don't include SSL support
      #
      AC_MSG_RESULT([disabled])
      ;;

    "bundled"|"yes")
      #
      # Use the bundled SSL implementation (yaSSL)
      #
      MYSQL_USE_BUNDLED_YASSL
      ;;

     *)
      #
      # A location where to search for OpenSSL was specified
      #
      MYSQL_FIND_OPENSSL([$mysql_ssl_dir])
      ;;
  esac
  AM_CONDITIONAL([HAVE_YASSL], [ test "$with_bundled_yassl" = "yes" ])
])
