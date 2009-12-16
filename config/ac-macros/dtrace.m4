dnl ---------------------------------------------------------------------------
dnl Macro: DTRACE_TEST
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(dtrace,
        AC_HELP_STRING([--enable-dtrace],[Build with support for the DTRACE.]),
        [
                ENABLE_DTRACE="$enable_dtrace"
        ],
        [
                ENABLE_DTRACE="yes" 
        ]
)
DTRACEFLAGS=""
HAVE_DTRACE=""
HAVE_DTRACE_DASH_G=""
if test "$ENABLE_DTRACE" = "yes"; then
  AC_PATH_PROGS(DTRACE, dtrace, [not found], [$PATH:/usr/sbin])
  if test "$DTRACE" = "not found"; then
    ENABLE_DTRACE="no"
  else
    AC_DEFINE([HAVE_DTRACE], [1], [Defined to 1 if DTrace support is enabled])
    case "$target_os" in
      *solaris*)
        HAVE_DTRACE_DASH_G="yes"
        ;;
      *)
        HAVE_DTRACE_DASH_G="no"
        ;;
    esac
  fi
fi
AC_SUBST(DTRACEFLAGS)
AC_SUBST(HAVE_DTRACE)
AM_CONDITIONAL([HAVE_DTRACE], [ test "$ENABLE_DTRACE" = "yes" ])
AM_CONDITIONAL([HAVE_DTRACE_DASH_G], [ test "$HAVE_DTRACE_DASH_G" = "yes" ])
dnl ---------------------------------------------------------------------------
dnl End Macro: DTRACE_TEST
dnl ---------------------------------------------------------------------------
