# Check if the function is available.
# HAVE_XXX will be defined if yes.

# $1: the name of function
# $2: the headers in where the function declared
AC_DEFUN([AX_CHECK_DECL_EX], [dnl
	AS_IF([test "x$2" = "x"], [AC_MSG_ERROR([header not privided])])
	AS_VAR_PUSHDEF([have_func_var], [HAVE_[]m4_toupper($1)])
	AC_CHECK_DECL([$1],dnl
		[AC_DEFINE([have_func_var], [1], [Define to 1 if you have the `$1' function.])],,dnl
		[$2]dnl
	)
	AS_VAR_POPDEF([have_func_var])dnl
])

AC_DEFUN([AX_CHECK_DECLS_EX], [dnl
	AS_IF([test "x$2" = "x"], [AC_MSG_ERROR([header not privided])])
	m4_foreach([decl],dnl
		m4_split(m4_normalize($1)),dnl
		[AX_CHECK_DECL_EX([decl], [$2])]dnl
	)
])
