dnl ######################################################################
dnl OpenSSL support
AC_DEFUN([LIBEVENT_OPENSSL], [
AC_REQUIRE([NTP_PKG_CONFIG])dnl

case "$enable_openssl" in
 yes)
    have_openssl=no
    case "$PKG_CONFIG" in
     '')
	;;
     *)
	OPENSSL_LIBS=`$PKG_CONFIG --libs openssl 2>/dev/null`
	case "$OPENSSL_LIBS" in
	 '') ;;
	 *) OPENSSL_LIBS="$OPENSSL_LIBS $EV_LIB_GDI $EV_LIB_WS32 $OPENSSL_LIBADD"
	    have_openssl=yes
	    ;;
	esac
	OPENSSL_INCS=`$PKG_CONFIG --cflags openssl 2>/dev/null`
	;;
    esac
    case "$have_openssl" in
     yes) ;;
     *)
	save_LIBS="$LIBS"
	LIBS=""
	OPENSSL_LIBS=""
	for lib in crypto eay32; do
		# clear cache
		unset ac_cv_search_SSL_new
		AC_SEARCH_LIBS([SSL_new], [ssl ssl32],
		    [have_openssl=yes
		    OPENSSL_LIBS="$LIBS -l$lib $EV_LIB_GDI $EV_LIB_WS32 $OPENSSL_LIBADD"],
		    [have_openssl=no],
		    [-l$lib $EV_LIB_GDI $EV_LIB_WS32 $OPENSSL_LIBADD])
		LIBS="$save_LIBS"
		test "$have_openssl" = "yes" && break
	done
	;;
    esac
    CPPFLAGS_SAVE=$CPPFLAGS
    CPPFLAGS="$CPPFLAGS $OPENSSL_INCS"
    AC_CHECK_HEADERS([openssl/ssl.h], [], [have_openssl=no])
    CPPFLAGS=$CPPFLAGS_SAVE
    AC_SUBST(OPENSSL_INCS)
    AC_SUBST(OPENSSL_LIBS)
    case "$have_openssl" in
     yes)  AC_DEFINE(HAVE_OPENSSL, 1, [Define if the system has openssl]) ;;
     *) AC_MSG_ERROR([openssl is a must but can not be found. You should add the \
directory containing `openssl.pc' to the `PKG_CONFIG_PATH' environment variable, \
or set `CFLAGS' and `LDFLAGS' directly for openssl, or use `--disable-openssl' \
to disable support for openssl encryption])
	;;
    esac
    ;;
esac

# check if we have and should use openssl
AM_CONDITIONAL(OPENSSL, [test "$enable_openssl" != "no" && test "$have_openssl" = "yes"])
])
