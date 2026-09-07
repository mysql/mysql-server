AC_INIT([libtirpc],[1.3.7])
AM_INIT_AUTOMAKE([silent-rules])
AM_SILENT_RULES([yes])
AC_CONFIG_SRCDIR([src/auth_des.c])
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_CC

# LT_VERSION_INFO="current:revision:age"
#
# From the libtool manual:
#
# 1.  Start with version information of 0:0:0 for each libtool library.
# 2.  Update the version information only immediately before a public
#     release of your software. More frequent updates are unnecessary,
#     and only guarantee that the current interface number gets larger faster.
# 3.  If the library source code has changed at all since the last update,
#     then increment revision (c:r:a becomes c:r+1:a).
# 4.  If any interfaces have been added, removed, or changed since the last
#     update, increment current, and set revision to 0.
# 5.  If any interfaces have been added since the last public release,
#     then increment age.
# 6.  If any interfaces have been removed since the last public release,
#     then set age to 0.
#
# _Never_ try to set the interface numbers so that they correspond to the
# release number of your package. This is an abuse that only fosters
# misunderstanding of the purpose of library versions.
#
# In addition to these rules, symbol versioning is now in effect. soname
# changes should be avoided.
#
LT_VERSION_INFO="3:0:0"
AC_SUBST([LT_VERSION_INFO])

AC_CHECK_HEADER([gssapi/gssapi.h], [HAVE_GSSAPI_H=yes], [HAVE_GSSAPI_H=no])

AC_ARG_ENABLE(gssapi,
	[AS_HELP_STRING([--disable-gssapi],[Disable GSSAPI support @<:@default=no@:>@])],
      [],[enable_gssapi=yes])
AM_CONDITIONAL(GSS, test "x$enable_gssapi" = xyes)

if test "x$enable_gssapi" = xyes; then
	if test "x$HAVE_GSSAPI_H" = xno; then
		AC_MSG_ERROR([gssapi.h not found. Use --disable-gssapi, or install GSS-API.])
	fi
	AC_CHECK_TOOL([KRB5_CONFIG], [krb5-config], [no])
	if test "x$KRB5_CONFIG" = xno; then
		AC_MSG_ERROR([krb5-config tool not found. Use --disable-gssapi, or install Kerberos.])
	fi
	GSSAPI_CFLAGS=`${KRB5_CONFIG} --cflags gssapi`
	GSSAPI_LIBS=`${KRB5_CONFIG} --libs gssapi`
	AC_SUBST([GSSAPI_CFLAGS])
	AC_SUBST([GSSAPI_LIBS])

    gssapi_save_CPPFLAGS="$CPPFLAGS"
    gssapi_save_LIBS="$LIBS"
    CPPFLAGS="$GSSAPI_CPPFLAGS $CPPFLAGS"
    LIBS="$GSSAPI_LIBS $LIBS"
    AC_CHECK_FUNCS([gss_pname_to_uid])
    CPPFLAGS="$gssapi_save_CPPFLAGS"
    LIBS="$gssapi_save_LIBS"
fi

AC_ARG_ENABLE(authdes,
	[AS_HELP_STRING([--enable-authdes],[Enable AUTH_DES support @<:@default=no@:>@])],
      [],[enable_authdes=no])
AM_CONDITIONAL(AUTHDES, test "x$enable_authdes" = xyes)
if test "x$enable_authdes" != xno; then
	AC_DEFINE(AUTHDES_SUPPORT, 1, [Define if AUTH_DES is support])
fi

AC_ARG_ENABLE(ipv6,
	[AS_HELP_STRING([--disable-ipv6],[Disable IPv6 support @<:@default=no@:>@])],
	[],[enable_ipv6=yes])
AM_CONDITIONAL(INET6, test "x$enable_ipv6" != xno)
if test "x$enable_ipv6" != xno; then
	AC_DEFINE(INET6, 1, [Define to 1 if IPv6 is available])
fi

# RPC database support
AC_ARG_ENABLE(rpcdb,
    [AS_HELP_STRING([--enable-rpcdb], [Enable RPC Database support @<:@default=no@:>@])],
    [], [enable_rpcdb=no])
AM_CONDITIONAL(RPCDB, test "x$enable_rpcdb" = xyes)
if test "x$enable_rpcdb" != "xno"; then
    AC_CHECK_FUNCS([getrpcent getrpcbyname getrpcbynumber], [have_rpcdb=yes])

    if test "x$have_rpcdb" = "xyes"; then
        AC_DEFINE([RPCDB], [1], [Define if RPC database support is available])
    fi
fi

AC_ARG_ENABLE(symvers,
	[AS_HELP_STRING([--disable-symvers],[Disable symbol versioning @<:@default=no@:>@])],
      [],[enable_symvers=maybe])

if test "x$enable_symvers" = xmaybe; then
   AC_MSG_CHECKING(if version scripts are supported)
   check_vscript_save_flags="$LDFLAGS"
   echo "V1 { global: show; local: *; };" > conftest.map
   AS_IF([test x = xyes], [echo "{" >> conftest.map])
   LDFLAGS="$LDFLAGS -Wl,--version-script,conftest.map"
   AC_LINK_IFELSE([AC_LANG_PROGRAM([[int show, hide;]], [])], [
   enable_symvers=yes
   AC_MSG_RESULT(yes)
   ], [AC_MSG_RESULT(no)])
   LDFLAGS="$check_vscript_save_flags"
   rm -f conftest.map
fi

AM_CONDITIONAL(SYMVERS, test "x$enable_symvers" = xyes)

# Generate symbol lists for version script
if test "x$enable_gssapi" = "xyes"; then
    GSS_SYMBOLS="_svcauth_gss; authgss_create; authgss_create_default; authgss_free_private_data; authgss_get_private_data; authgss_service; gss_log_debug; gss_log_hexdump; gss_log_status; rpc_gss_get_error; rpc_gss_get_mech_info; rpc_gss_get_mechanisms; rpc_gss_get_principal_name; rpc_gss_get_versions; rpc_gss_qop_to_num; rpc_gss_seccreate; rpc_gss_set_callback; rpc_gss_set_defaults; rpc_gss_set_svc_name; rpc_gss_svc_max_data_length;"

    GSS_SYMBOLS_031="svcauth_gss_get_principal; svcauth_gss_set_svc_name;"
else
    GSS_SYMBOLS=""
    GSS_SYMBOLS_031=""
fi

if test "x$enable_authdes" = "xyes"; then
    DES_SYMBOLS="cbc_crypt; ecb_crypt; xdr_authdes_cred; xdr_authdes_verf; xdr_rpc_gss_cred; xdr_rpc_gss_data; xdr_rpc_gss_init_args; xdr_rpc_gss_init_res;"
else
    DES_SYMBOLS=""
fi

if test "x$enable_rpcdb" = "xyes"; then
    RPCDB_SYMBOLS="endrpcent; getrpcent; getrpcbynumber; getrpcbyname; setrpcent;"
else
    RPCDB_SYMBOLS=""
fi

AC_SUBST([GSS_SYMBOLS])
AC_SUBST([GSS_SYMBOLS_031])
AC_SUBST([DES_SYMBOLS])
AC_SUBST([RPCDB_SYMBOLS])

AC_CANONICAL_BUILD
# Check for which host we are on and setup a few things
# specifically based on the host
case $build_os in
     linux*)
         # Do something specific for linux
         LDFLAG_NOUNDEFINED="-Wl,--no-undefined"
         AC_SUBST(LDFLAG_NOUNDEFINED)
         ;;
     *)
         #Default Case
         ;;
esac


AC_MSG_CHECKING(for SOL_IP)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <netdb.h>]], [[
    int ipproto = SOL_IP;
]])],[
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_SOL_IP, 1, [Have SOL_IP])
],[
    AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for SOL_IPV6)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <netdb.h>]], [[
    int ipproto = SOL_IPV6;
]])],[
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_SOL_IPV6, 1, [Have SOL_IPV6])
],[
    AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for IPPROTO_IP)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <netinet/in.h>]], [[
    int ipproto = IPPROTO_IP;
]])],[
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_IPPROTO_IP, 1, [Have IPPROTO_IP])
],[
    AC_MSG_RESULT(no)
])

AC_MSG_CHECKING(for IPPROTO_IPV6)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <netinet/in.h>]], [[
    int ipproto = IPPROTO_IPV6;
]])],[
    AC_MSG_RESULT(yes)
    AC_DEFINE(HAVE_IPPROTO_IPV6, 1, [Have IPPROTO_IPV6])
],[
    AC_MSG_RESULT(no)
])
AC_MSG_CHECKING([for IPV6_PKTINFO])

AC_CONFIG_HEADERS([config.h])
LT_INIT
AC_HEADER_DIRENT
AC_PREFIX_DEFAULT(/usr)
AC_CHECK_HEADERS([arpa/inet.h fcntl.h libintl.h limits.h locale.h
netdb.h netinet/in.h stddef.h stdint.h stdlib.h string.h sys/ioctl.h
sys/param.h sys/socket.h sys/time.h syslog.h unistd.h features.h
gssapi/gssapi_ext.h endian.h machine/endian.h])
AX_PTHREAD
AC_CHECK_FUNCS([getpeereid getrpcbyname getrpcbynumber setrpcent endrpcent getrpcent])

AC_CHECK_TYPES(struct rpcent,,, [
      #include <netdb.h>])
AC_CONFIG_FILES([Makefile src/Makefile man/Makefile doc/Makefile])
AC_CONFIG_FILES([src/libtirpc.map])
AC_CONFIG_FILES([libtirpc.pc])
AC_OUTPUT

# Configuration summary
AC_MSG_NOTICE([
libtirpc configuration summary:
  GSS-API support: $enable_gssapi
  DES crypto support: $enable_authdes
  RPC database support: $enable_rpcdb
  Symbol versioning: $enable_symvers
])

