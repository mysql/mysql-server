dnl ---------------------------------------------------------------------------
dnl Macro: MYSQL_STORAGE_ENGINE
dnl
dnl What it does:
dnl   creates --with-xxx configure option
dnl   adds HAVE_XXX to config.h
dnl   appends &xxx_hton, to the list of hanldertons
dnl   appends a dir to the list of source directories
dnl   appends ha_xxx.cc to the list of handler files
dnl
dnl  all names above are configurable with reasonable defaults.
dnl
dnl ---------------------------------------------------------------------------

AC_DEFUN([MYSQL_STORAGE_ENGINE],
[_MYSQL_STORAGE_ENGINE(
[$1],                                dnl name
m4_default([$2], [$1 storage engine]),    dnl verbose name
m4_default([$3], [$1-storage-engine]),    dnl with-name
m4_default([$4], no),                     dnl default
m4_default([$5], [WITH_]AS_TR_CPP([$1])[_STORAGE_ENGINE]),
m4_default([$6], $1[_hton]),              dnl hton
m4_default([$7], []),                     dnl path to the code
m4_default([$8], [ha_$1.o]),             dnl path to the handler in
m4_default([$9], []),                    dnl path to extra libraries
[$10],                                     dnl code-if-set
)])

AC_DEFUN([_MYSQL_STORAGE_ENGINE],
[
AC_ARG_WITH([$3], AS_HELP_STRING([--with-$3], [enable $2 (default is $4)]),
[], [ [with_]m4_bpatsubst([$3], -, _)=['$4']])
AC_CACHE_CHECK([whether to use $2], [mysql_cv_use_]m4_bpatsubst([$3], -, _),
[mysql_cv_use_]m4_bpatsubst([$3], -, _)=[$with_]m4_bpatsubst([$3], -, _))
AH_TEMPLATE([$5], [Build $2])
if test "[$mysql_cv_use_]m4_bpatsubst([$3], -, _)" != no; then
if test "$6" != "no"
then
  AC_DEFINE([$5])
  mysql_se_decls="${mysql_se_decls},$6"
  mysql_se_htons="${mysql_se_htons},&$6"
  mysql_se_objs="$mysql_se_objs $8"
fi
mysql_se_dirs="$mysql_se_dirs $7"
mysql_se_libs="$mysql_se_libs $9"
$10
fi
])

dnl ---------------------------------------------------------------------------
