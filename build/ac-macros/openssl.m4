AC_DEFUN([MYSQL_FIND_OPENSSL], [
  incs="$1"
  libs="$2"
  case "$incs---$libs" in
    ---)
      for d in /usr/ssl/include /usr/local/ssl/include /usr/include \
/usr/include/ssl /opt/ssl/include /opt/openssl/include \
/usr/local/ssl/include /usr/local/include /usr/freeware/include ; do
       if test -f $d/openssl/ssl.h  ; then
         OPENSSL_INCLUDE=-I$d
       fi
      done

      for d in /usr/ssl/lib /usr/local/ssl/lib /usr/lib/openssl \
/usr/lib /usr/lib64 /opt/ssl/lib /opt/openssl/lib \
/usr/freeware/lib32 /usr/local/lib/ ; do
      if test -f $d/libssl.a || test -f $d/libssl.so || test -f $d/libssl.dylib ; then
        OPENSSL_LIB=$d
      fi
      done
      ;;
    ---* | *---)
      AC_MSG_ERROR([if either 'includes' or 'libs' is specified, both must be specified])
      ;;
    * )
      if test -f $incs/openssl/ssl.h  ; then
        OPENSSL_INCLUDE=-I$incs
      fi
      if test -f $libs/libssl.a || test -f $libs/libssl.so || test -f $libs/libssl.dylib ; then
        OPENSSL_LIB=$libs
      fi
      ;;
  esac

  # On RedHat 9 we need kerberos to compile openssl
  for d in /usr/kerberos/include
  do
   if test -f $d/krb5.h  ; then
     OPENSSL_KERBEROS_INCLUDE="$d"
   fi
  done


 if test -z "$OPENSSL_LIB" -o -z "$OPENSSL_INCLUDE" ; then
   echo "Could not find an installation of OpenSSL"
   if test -n "$OPENSSL_LIB" ; then
    if test "$IS_LINUX" = "true"; then
      echo "Looks like you've forgotten to install OpenSSL development RPM"
    fi
   fi
  exit 1
 fi

])

AC_DEFUN([MYSQL_CHECK_OPENSSL], [
AC_MSG_CHECKING(for OpenSSL)
  AC_ARG_WITH([openssl],
              [  --with-openssl[=DIR]    Include the OpenSSL support],
              [openssl="$withval"],
              [openssl=no])

  AC_ARG_WITH([openssl-includes],
              [
  --with-openssl-includes=DIR
                          Find OpenSSL headers in DIR],
              [openssl_includes="$withval"],
              [openssl_includes=""])

  AC_ARG_WITH([openssl-libs],
              [
  --with-openssl-libs=DIR
                          Find OpenSSL libraries in DIR],
              [openssl_libs="$withval"],
              [openssl_libs=""])

  if test "$openssl" != "no"
  then
	if test "$openssl" != "yes"
	then
		if test -z "$openssl_includes" 
		then
			openssl_includes="$openssl/include"
		fi
		if test -z "$openssl_libs" 
		then
			openssl_libs="$openssl/lib"
		fi
	fi
    MYSQL_FIND_OPENSSL([$openssl_includes], [$openssl_libs])
    #force VIO use
    vio_dir="vio"
    vio_libs="../vio/libvio.la"
    AC_DEFINE([HAVE_VIO], [1], [Virtual IO])
    AC_MSG_RESULT(yes)
    openssl_libs="-L$OPENSSL_LIB -lssl -lcrypto"
    # Don't set openssl_includes to /usr/include as this gives us a lot of
    # compiler warnings when using gcc 3.x
    openssl_includes=""
    if test "$OPENSSL_INCLUDE" != "-I/usr/include"
    then
	openssl_includes="$OPENSSL_INCLUDE"
    fi
    if test "$OPENSSL_KERBEROS_INCLUDE"
    then
    	openssl_includes="$openssl_includes -I$OPENSSL_KERBEROS_INCLUDE"
    fi
    AC_DEFINE([HAVE_OPENSSL], [1], [OpenSSL])

    # openssl-devel-0.9.6 requires dlopen() and we can't link staticly
    # on many platforms (We should actually test this here, but it's quite
    # hard) to do as we are doing libtool for linking.
    using_static=""
    case "$CLIENT_EXTRA_LDFLAGS $MYSQLD_EXTRA_LDFLAGS" in
	*-all-static*) using_static="yes" ;;
    esac
    if test "$using_static" = "yes"
    then
      echo "You can't use the --all-static link option when using openssl."
      exit 1
    fi
    NON_THREADED_CLIENT_LIBS="$NON_THREADED_CLIENT_LIBS $openssl_libs"
  else
    AC_MSG_RESULT(no)
	if test ! -z "$openssl_includes"
	then
		AC_MSG_ERROR(Can't have --with-openssl-includes without --with-openssl);
	fi
	if test ! -z "$openssl_libs"
	then
		AC_MSG_ERROR(Can't have --with-openssl-libs without --with-openssl);
	fi
  fi
  AC_SUBST(openssl_libs)
  AC_SUBST(openssl_includes)
])
