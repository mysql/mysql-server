AC_DEFUN([MYSQL_CHECK_ISAM], [
  AC_ARG_WITH([isam], [
  --with-isam             Enable the ISAM table type],
    [with_isam="$withval"],
    [with_isam=no])

  isam_libs=
  if test X"$with_isam" = X"yes"
  then
    AC_DEFINE([HAVE_ISAM], [1], [Using old ISAM tables])
    isam_libs="\$(top_builddir)/isam/libnisam.a\
 \$(top_builddir)/merge/libmerge.a"
  fi
  AC_SUBST(isam_libs)
])
