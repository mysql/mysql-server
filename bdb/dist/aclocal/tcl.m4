dnl $Id: tcl.m4,v 11.5 2000/06/27 13:21:28 bostic Exp $

dnl The SC_* macros in this file are from the unix/tcl.m4 files in the Tcl
dnl 8.3.0 distribution, with some minor changes.  For this reason, license
dnl terms for the Berkeley DB distribution dist/aclocal/tcl.m4 file are as
dnl follows (copied from the license.terms file in the Tcl 8.3 distribution):
dnl
dnl This software is copyrighted by the Regents of the University of
dnl California, Sun Microsystems, Inc., Scriptics Corporation,
dnl and other parties.  The following terms apply to all files associated
dnl with the software unless explicitly disclaimed in individual files.
dnl 
dnl The authors hereby grant permission to use, copy, modify, distribute,
dnl and license this software and its documentation for any purpose, provided
dnl that existing copyright notices are retained in all copies and that this
dnl notice is included verbatim in any distributions. No written agreement,
dnl license, or royalty fee is required for any of the authorized uses.
dnl Modifications to this software may be copyrighted by their authors
dnl and need not follow the licensing terms described here, provided that
dnl the new terms are clearly indicated on the first page of each file where
dnl they apply.
dnl 
dnl IN NO EVENT SHALL THE AUTHORS OR DISTRIBUTORS BE LIABLE TO ANY PARTY
dnl FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
dnl ARISING OUT OF THE USE OF THIS SOFTWARE, ITS DOCUMENTATION, OR ANY
dnl DERIVATIVES THEREOF, EVEN IF THE AUTHORS HAVE BEEN ADVISED OF THE
dnl POSSIBILITY OF SUCH DAMAGE.
dnl 
dnl THE AUTHORS AND DISTRIBUTORS SPECIFICALLY DISCLAIM ANY WARRANTIES,
dnl INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
dnl FITNESS FOR A PARTICULAR PURPOSE, AND NON-INFRINGEMENT.  THIS SOFTWARE
dnl IS PROVIDED ON AN "AS IS" BASIS, AND THE AUTHORS AND DISTRIBUTORS HAVE
dnl NO OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
dnl MODIFICATIONS.
dnl 
dnl GOVERNMENT USE: If you are acquiring this software on behalf of the
dnl U.S. government, the Government shall have only "Restricted Rights"
dnl in the software and related documentation as defined in the Federal 
dnl Acquisition Regulations (FARs) in Clause 52.227.19 (c) (2).  If you
dnl are acquiring the software on behalf of the Department of Defense, the
dnl software shall be classified as "Commercial Computer Software" and the
dnl Government shall have only "Restricted Rights" as defined in Clause
dnl 252.227-7013 (c) (1) of DFARs.  Notwithstanding the foregoing, the
dnl authors grant the U.S. Government and others acting in its behalf
dnl permission to use and distribute the software in accordance with the
dnl terms specified in this license. 

AC_DEFUN(SC_PATH_TCLCONFIG, [
	AC_CACHE_VAL(ac_cv_c_tclconfig,[

	    # First check to see if --with-tclconfig was specified.
	    if test "${with_tclconfig}" != no; then
		if test -f "${with_tclconfig}/tclConfig.sh" ; then
		    ac_cv_c_tclconfig=`(cd ${with_tclconfig}; pwd)`
		else
		    AC_MSG_ERROR([${with_tclconfig} directory doesn't contain tclConfig.sh])
		fi
	    fi

	    # check in a few common install locations
	    if test x"${ac_cv_c_tclconfig}" = x ; then
		for i in `ls -d /usr/local/lib 2>/dev/null` ; do
		    if test -f "$i/tclConfig.sh" ; then
			ac_cv_c_tclconfig=`(cd $i; pwd)`
			break
		    fi
		done
	    fi

	])

	if test x"${ac_cv_c_tclconfig}" = x ; then
	    TCL_BIN_DIR="# no Tcl configs found"
	    AC_MSG_ERROR(can't find Tcl configuration definitions)
	else
	    TCL_BIN_DIR=${ac_cv_c_tclconfig}
	fi
])

AC_DEFUN(SC_LOAD_TCLCONFIG, [
	AC_MSG_CHECKING([for existence of $TCL_BIN_DIR/tclConfig.sh])

	if test -f "$TCL_BIN_DIR/tclConfig.sh" ; then
		AC_MSG_RESULT([loading])
		. $TCL_BIN_DIR/tclConfig.sh
	else
		AC_MSG_RESULT([file not found])
	fi

	#
	# The eval is required to do the TCL_DBGX substitution in the
	# TCL_LIB_FILE variable
	#
	eval TCL_LIB_FILE="${TCL_LIB_FILE}"
	eval TCL_LIB_FLAG="${TCL_LIB_FLAG}"
	eval "TCL_LIB_SPEC=\"${TCL_LIB_SPEC}\""

	AC_SUBST(TCL_BIN_DIR)
	AC_SUBST(TCL_SRC_DIR)
	AC_SUBST(TCL_LIB_FILE)

	AC_SUBST(TCL_TCLSH)
	TCL_TCLSH="${TCL_PREFIX}/bin/tclsh${TCL_VERSION}"
])

dnl Optional Tcl API.
AC_DEFUN(AM_TCL_LOAD, [
if test "$db_cv_tcl" != no; then
	if test "$db_cv_dynamic" != "yes"; then
		AC_MSG_ERROR([--with-tcl requires --enable-dynamic])
	fi

	AC_SUBST(TCFLAGS)

	SC_PATH_TCLCONFIG
	SC_LOAD_TCLCONFIG

	if test x"$TCL_PREFIX" != x && test -f "$TCL_PREFIX/include/tcl.h"; then
		TCFLAGS="-I$TCL_PREFIX/include"
	fi

	LIBS="$LIBS $TCL_LIB_SPEC $TCL_LIBS"

	ADDITIONAL_LIBS="$ADDITIONAL_LIBS \$(libtso_target)"
	DEFAULT_INSTALL="${DEFAULT_INSTALL} install_tcl"
fi])
