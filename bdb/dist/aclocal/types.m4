dnl $Id: types.m4,v 11.4 1999/12/04 19:18:28 bostic Exp $

dnl Check for the standard shorthand types.
AC_DEFUN(AM_SHORTHAND_TYPES, [dnl

AC_SUBST(ssize_t_decl)
AC_CACHE_CHECK([for ssize_t], db_cv_ssize_t, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], ssize_t foo;,
	[db_cv_ssize_t=yes], [db_cv_ssize_t=no])])
if test "$db_cv_ssize_t" = no; then
	ssize_t_decl="typedef int ssize_t;"
fi

AC_SUBST(u_char_decl)
AC_CACHE_CHECK([for u_char], db_cv_uchar, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], u_char foo;,
	[db_cv_uchar=yes], [db_cv_uchar=no])])
if test "$db_cv_uchar" = no; then
	u_char_decl="typedef unsigned char u_char;"
fi

AC_SUBST(u_short_decl)
AC_CACHE_CHECK([for u_short], db_cv_ushort, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], u_short foo;,
	[db_cv_ushort=yes], [db_cv_ushort=no])])
if test "$db_cv_ushort" = no; then
	u_short_decl="typedef unsigned short u_short;"
fi

AC_SUBST(u_int_decl)
AC_CACHE_CHECK([for u_int], db_cv_uint, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], u_int foo;,
	[db_cv_uint=yes], [db_cv_uint=no])])
if test "$db_cv_uint" = no; then
	u_int_decl="typedef unsigned int u_int;"
fi

AC_SUBST(u_long_decl)
AC_CACHE_CHECK([for u_long], db_cv_ulong, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], u_long foo;,
	[db_cv_ulong=yes], [db_cv_ulong=no])])
if test "$db_cv_ulong" = no; then
	u_long_decl="typedef unsigned long u_long;"
fi

dnl DB/Vi use specific integer sizes.
AC_SUBST(u_int8_decl)
AC_CACHE_CHECK([for u_int8_t], db_cv_uint8, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], u_int8_t foo;,
	[db_cv_uint8=yes],
	AC_TRY_RUN([main(){exit(sizeof(unsigned char) != 1);}],
	    [db_cv_uint8="unsigned char"], [db_cv_uint8=no]))])
if test "$db_cv_uint8" = no; then
	AC_MSG_ERROR(No unsigned 8-bit integral type.)
fi
if test "$db_cv_uint8" != yes; then
	u_int8_decl="typedef $db_cv_uint8 u_int8_t;"
fi

AC_SUBST(u_int16_decl)
AC_CACHE_CHECK([for u_int16_t], db_cv_uint16, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], u_int16_t foo;,
	[db_cv_uint16=yes],
AC_TRY_RUN([main(){exit(sizeof(unsigned short) != 2);}],
	[db_cv_uint16="unsigned short"],
AC_TRY_RUN([main(){exit(sizeof(unsigned int) != 2);}],
	[db_cv_uint16="unsigned int"], [db_cv_uint16=no])))])
if test "$db_cv_uint16" = no; then
	AC_MSG_ERROR([No unsigned 16-bit integral type.])
fi
if test "$db_cv_uint16" != yes; then
	u_int16_decl="typedef $db_cv_uint16 u_int16_t;"
fi

AC_SUBST(int16_decl)
AC_CACHE_CHECK([for int16_t], db_cv_int16, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], int16_t foo;,
	[db_cv_int16=yes],
AC_TRY_RUN([main(){exit(sizeof(short) != 2);}],
	[db_cv_int16="short"],
AC_TRY_RUN([main(){exit(sizeof(int) != 2);}],
	[db_cv_int16="int"], [db_cv_int16=no])))])
if test "$db_cv_int16" = no; then
	AC_MSG_ERROR([No signed 16-bit integral type.])
fi
if test "$db_cv_int16" != yes; then
	int16_decl="typedef $db_cv_int16 int16_t;"
fi

AC_SUBST(u_int32_decl)
AC_CACHE_CHECK([for u_int32_t], db_cv_uint32, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], u_int32_t foo;,
	[db_cv_uint32=yes],
AC_TRY_RUN([main(){exit(sizeof(unsigned int) != 4);}],
	[db_cv_uint32="unsigned int"],
AC_TRY_RUN([main(){exit(sizeof(unsigned long) != 4);}],
	[db_cv_uint32="unsigned long"], [db_cv_uint32=no])))])
if test "$db_cv_uint32" = no; then
	AC_MSG_ERROR([No unsigned 32-bit integral type.])
fi
if test "$db_cv_uint32" != yes; then
	u_int32_decl="typedef $db_cv_uint32 u_int32_t;"
fi

AC_SUBST(int32_decl)
AC_CACHE_CHECK([for int32_t], db_cv_int32, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], int32_t foo;,
	[db_cv_int32=yes],
AC_TRY_RUN([main(){exit(sizeof(int) != 4);}],
	[db_cv_int32="int"],
AC_TRY_RUN([main(){exit(sizeof(long) != 4);}],
	[db_cv_int32="long"], [db_cv_int32=no])))])
if test "$db_cv_int32" = no; then
	AC_MSG_ERROR([No signed 32-bit integral type.])
fi
if test "$db_cv_int32" != yes; then
	int32_decl="typedef $db_cv_int32 int32_t;"
fi

dnl Figure out largest integral type.
AC_SUBST(db_align_t_decl)
AC_CACHE_CHECK([for largest integral type], db_cv_align_t, [dnl
AC_TRY_COMPILE([#include <sys/types.h>], long long foo;,
	[db_cv_align_t="unsigned long long"], [db_cv_align_t="unsigned long"])])
db_align_t_decl="typedef $db_cv_align_t db_align_t;"

dnl Figure out integral type the same size as a pointer.
AC_SUBST(db_alignp_t_decl)
AC_CACHE_CHECK([for integral type equal to pointer size], db_cv_alignp_t, [dnl
db_cv_alignp_t=$db_cv_align_t
AC_TRY_RUN([main(){exit(sizeof(unsigned int) != sizeof(char *));}],
	[db_cv_alignp_t="unsigned int"])
AC_TRY_RUN([main(){exit(sizeof(unsigned long) != sizeof(char *));}],
	[db_cv_alignp_t="unsigned long"])
AC_TRY_RUN([main(){exit(sizeof(unsigned long long) != sizeof(char *));}],
	[db_cv_alignp_t="unsigned long long"])])
db_alignp_t_decl="typedef $db_cv_alignp_t db_alignp_t;"

])dnl
