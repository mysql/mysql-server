#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

###############################################################################
#
# Author: Lasse Collin
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
#
###############################################################################

# NOTE: Don't add useless checks. autoscan detects this and that, but don't
# let it confuse you. For example, we don't care about checking for behavior
# of malloc(), stat(), or lstat(), since we don't use those functions in
# a way that would cause the problems the autoconf macros check.

AC_PREREQ([2.61])

AC_INIT([XZ Utils], m4_esyscmd([/bin/sh version.sh]),
	[lasse.collin@tukaani.org], [xz])
AC_CONFIG_SRCDIR([src/liblzma/common/common.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADER([config.h])

PACKAGE_HOMEPAGE=http://tukaani.org/xz/
AC_DEFINE_UNQUOTED([PACKAGE_HOMEPAGE], ["$PACKAGE_HOMEPAGE"],
	[Define to the URL of the home page of this package.])
AC_SUBST([PACKAGE_HOMEPAGE])

echo
echo "$PACKAGE_STRING"

echo
echo "System type:"
# This is needed to know if assembler optimizations can be used.
AC_CANONICAL_HOST

# We do some special things on Windows (32-bit or 64-bit) builds.
case $host_os in
	mingw* | cygwin*) is_w32=yes ;;
	*)                is_w32=no ;;
esac
AM_CONDITIONAL([COND_W32], [test "$is_w32" = yes])


echo
echo "Configure options:"
AM_CFLAGS=


#############
# Debugging #
#############

AC_MSG_CHECKING([if debugging code should be compiled])
AC_ARG_ENABLE([debug], AC_HELP_STRING([--enable-debug], [Enable debugging code.]),
	[], enable_debug=no)
if test "x$enable_debug" = xyes; then
	AC_MSG_RESULT([yes])
else
	AC_DEFINE([NDEBUG], [1], [Define to 1 to disable debugging code.])
	AC_MSG_RESULT([no])
fi


###########
# Filters #
###########

m4_define([SUPPORTED_FILTERS], [lzma1,lzma2,subblock,delta,x86,powerpc,ia64,arm,armthumb,sparc])dnl
m4_define([SIMPLE_FILTERS], [x86,powerpc,ia64,arm,armthumb,sparc])
m4_define([LZ_FILTERS], [lzma1,lzma2])

m4_foreach([NAME], [SUPPORTED_FILTERS],
[enable_filter_[]NAME=no
enable_encoder_[]NAME=no
enable_decoder_[]NAME=no
])dnl

AC_MSG_CHECKING([which encoders to build])
AC_ARG_ENABLE([encoders], AC_HELP_STRING([--enable-encoders=LIST],
		[Comma-separated list of encoders to build. Default=all.
		Available encoders:]
			m4_translit(m4_defn([SUPPORTED_FILTERS]), [,], [ ])),
	[], [enable_encoders=SUPPORTED_FILTERS])
enable_encoders=`echo "$enable_encoders" | sed 's/,subblock//; s/,/ /g'`
if test "x$enable_encoders" = xno || test "x$enable_encoders" = x; then
	AC_MSG_RESULT([(none)])
else
	AC_DEFINE([HAVE_ENCODER], [1],
		[Define to 1 if encoder components are enabled.])
	for arg in $enable_encoders
	do
		case $arg in m4_foreach([NAME], [SUPPORTED_FILTERS], [
			NAME)
				enable_filter_[]NAME=yes
				enable_encoder_[]NAME=yes
				AC_DEFINE(HAVE_ENCODER_[]m4_toupper(NAME), [1],
				[Define to 1 if] NAME [encoder is enabled.])
				;;])
			*)
				AC_MSG_RESULT([])
				AC_MSG_ERROR([unknown filter: $arg])
				;;
		esac
	done
	AC_MSG_RESULT([$enable_encoders])
fi

AC_MSG_CHECKING([which decoders to build])
AC_ARG_ENABLE([decoders], AC_HELP_STRING([--enable-decoders=LIST],
		[Comma-separated list of decoders to build. Default=all.
		Available decoders are the same as available encoders.]),
	[], [enable_decoders=SUPPORTED_FILTERS])
enable_decoders=`echo "$enable_decoders" | sed 's/,subblock//; s/,/ /g'`
if test "x$enable_decoders" = xno || test "x$enable_decoders" = x; then
	AC_MSG_RESULT([(none)])
else
	AC_DEFINE([HAVE_DECODER], [1],
		[Define to 1 if decoder components are enabled.])
	for arg in $enable_decoders
	do
		case $arg in m4_foreach([NAME], [SUPPORTED_FILTERS], [
			NAME)
				enable_filter_[]NAME=yes
				enable_decoder_[]NAME=yes
				AC_DEFINE(HAVE_DECODER_[]m4_toupper(NAME), [1],
				[Define to 1 if] NAME [decoder is enabled.])
				;;])
			*)
				AC_MSG_RESULT([])
				AC_MSG_ERROR([unknown filter: $arg])
				;;
		esac
	done

	# LZMA2 requires that LZMA1 is enabled.
	test "x$enable_encoder_lzma2" = xyes && enable_encoder_lzma1=yes
	test "x$enable_decoder_lzma2" = xyes && enable_decoder_lzma1=yes

	AC_MSG_RESULT([$enable_decoders])
fi

if test "x$enable_encoder_lzma2$enable_encoder_lzma1" = xyesno \
		|| test "x$enable_decoder_lzma2$enable_decoder_lzma1" = xyesno; then
	AC_MSG_ERROR([LZMA2 requires that LZMA1 is also enabled.])
fi

AM_CONDITIONAL(COND_MAIN_ENCODER, test "x$enable_encoders" != xno && test "x$enable_encoders" != x)
AM_CONDITIONAL(COND_MAIN_DECODER, test "x$enable_decoders" != xno && test "x$enable_decoders" != x)

m4_foreach([NAME], [SUPPORTED_FILTERS],
[AM_CONDITIONAL(COND_FILTER_[]m4_toupper(NAME), test "x$enable_filter_[]NAME" = xyes)
AM_CONDITIONAL(COND_ENCODER_[]m4_toupper(NAME), test "x$enable_encoder_[]NAME" = xyes)
AM_CONDITIONAL(COND_DECODER_[]m4_toupper(NAME), test "x$enable_decoder_[]NAME" = xyes)
])dnl

# The so called "simple filters" share common code.
enable_filter_simple=no
enable_encoder_simple=no
enable_decoder_simple=no
m4_foreach([NAME], [SIMPLE_FILTERS],
[test "x$enable_filter_[]NAME" = xyes && enable_filter_simple=yes
test "x$enable_encoder_[]NAME" = xyes && enable_encoder_simple=yes
test "x$enable_decoder_[]NAME" = xyes && enable_decoder_simple=yes
])dnl
AM_CONDITIONAL(COND_FILTER_SIMPLE, test "x$enable_filter_simple" = xyes)
AM_CONDITIONAL(COND_ENCODER_SIMPLE, test "x$enable_encoder_simple" = xyes)
AM_CONDITIONAL(COND_DECODER_SIMPLE, test "x$enable_decoder_simple" = xyes)

# LZ-based filters share common code.
enable_filter_lz=no
enable_encoder_lz=no
enable_decoder_lz=no
m4_foreach([NAME], [LZ_FILTERS],
[test "x$enable_filter_[]NAME" = xyes && enable_filter_lz=yes
test "x$enable_encoder_[]NAME" = xyes && enable_encoder_lz=yes
test "x$enable_decoder_[]NAME" = xyes && enable_decoder_lz=yes
])dnl
AM_CONDITIONAL(COND_FILTER_LZ, test "x$enable_filter_lz" = xyes)
AM_CONDITIONAL(COND_ENCODER_LZ, test "x$enable_encoder_lz" = xyes)
AM_CONDITIONAL(COND_DECODER_LZ, test "x$enable_decoder_lz" = xyes)


#################
# Match finders #
#################

m4_define([SUPPORTED_MATCH_FINDERS], [hc3,hc4,bt2,bt3,bt4])

m4_foreach([NAME], [SUPPORTED_MATCH_FINDERS],
[enable_match_finder_[]NAME=no
])

AC_MSG_CHECKING([which match finders to build])
AC_ARG_ENABLE([match-finders], AC_HELP_STRING([--enable-match-finders=LIST],
		[Comma-separated list of match finders to build. Default=all.
		At least one match finder is required for encoding with
		the LZMA1 and LZMA2 filters. Available match finders:]
		m4_translit(m4_defn([SUPPORTED_MATCH_FINDERS]), [,], [ ])), [],
	[enable_match_finders=SUPPORTED_MATCH_FINDERS])
enable_match_finders=`echo "$enable_match_finders" | sed 's/,/ /g'`
if test "x$enable_encoder_lz" = xyes ; then
	for arg in $enable_match_finders
		do
		case $arg in m4_foreach([NAME], [SUPPORTED_MATCH_FINDERS], [
			NAME)
				enable_match_finder_[]NAME=yes
				AC_DEFINE(HAVE_MF_[]m4_toupper(NAME), [1],
				[Define to 1 to enable] NAME [match finder.])
				;;])
			*)
				AC_MSG_RESULT([])
				AC_MSG_ERROR([unknown match finder: $arg])
				;;
		esac
	done
	AC_MSG_RESULT([$enable_match_finders])
else
	AC_MSG_RESULT([(none because not building any LZ-based encoder)])
fi


####################
# Integrity checks #
####################

m4_define([SUPPORTED_CHECKS], [crc32,crc64,sha256])

m4_foreach([NAME], [SUPPORTED_FILTERS],
[enable_check_[]NAME=no
])dnl

AC_MSG_CHECKING([which integrity checks to build])
AC_ARG_ENABLE([checks], AC_HELP_STRING([--enable-checks=LIST],
		[Comma-separated list of integrity checks to build.
		Default=all. Available integrity checks:]
		m4_translit(m4_defn([SUPPORTED_CHECKS]), [,], [ ])),
	[], [enable_checks=SUPPORTED_CHECKS])
enable_checks=`echo "$enable_checks" | sed 's/,/ /g'`
if test "x$enable_checks" = xno || test "x$enable_checks" = x; then
	AC_MSG_RESULT([(none)])
else
	for arg in $enable_checks
	do
		case $arg in m4_foreach([NAME], [SUPPORTED_CHECKS], [
			NAME)
				enable_check_[]NAME=yes
				AC_DEFINE(HAVE_CHECK_[]m4_toupper(NAME), [1],
				[Define to 1 if] NAME
				[integrity check is enabled.])
				;;])
			*)
				AC_MSG_RESULT([])
				AC_MSG_ERROR([unknown integrity check: $arg])
				;;
		esac
	done
	AC_MSG_RESULT([$enable_checks])
fi
if test "x$enable_checks_crc32" = xno ; then
	AC_MSG_ERROR([For now, the CRC32 check must always be enabled.])
fi

m4_foreach([NAME], [SUPPORTED_CHECKS],
[AM_CONDITIONAL(COND_CHECK_[]m4_toupper(NAME), test "x$enable_check_[]NAME" = xyes)
])dnl


###########################
# Assembler optimizations #
###########################

AC_MSG_CHECKING([if assembler optimizations should be used])
AC_ARG_ENABLE([assembler], AC_HELP_STRING([--disable-assembler],
		[Do not use assembler optimizations even if such exist
		for the architecture.]),
	[], [enable_assembler=yes])
if test "x$enable_assembler" = xyes; then
	case $host_cpu in
		i?86)   enable_assembler=x86 ;;
		x86_64) enable_assembler=x86_64 ;;
		*)      enable_assembler=no ;;
	esac
fi
case $enable_assembler in
	x86)
		AC_DEFINE([HAVE_ASM_X86], [1],
			[Define to 1 if using x86 assembler optimizations.])
		;;
	x86_64)
		AC_DEFINE([HAVE_ASM_X86_64], [1],
			[Define to 1 if using x86_64 assembler optimizations.])
		;;
	no)
		;;
	*)
		AC_MSG_RESULT([])
		AC_MSG_ERROR([--enable-assembler accepts only \`yes', \`no', \`x86', or \`x86_64'.])
		;;
esac
AC_MSG_RESULT([$enable_assembler])
AM_CONDITIONAL(COND_ASM_X86, test "x$enable_assembler" = xx86)
AM_CONDITIONAL(COND_ASM_X86_64, test "x$enable_assembler" = xx86_64)


################################
# Fast unaligned memory access #
################################

AC_MSG_CHECKING([if unaligned memory access should be used])
AC_ARG_ENABLE([unaligned-access], AC_HELP_STRING([--enable-unaligned-access],
		[Enable if the system supports *fast* unaligned memory access
		with 16-bit and 32-bit integers. By default, this is enabled
		only on x86, x86_64, and big endian PowerPC.]),
	[], [enable_unaligned_access=auto])
if test "x$enable_unaligned_access" = xauto ; then
	case $host_cpu in
		i?86|x86_64|powerpc|powerpc64)
			enable_unaligned_access=yes
			;;
		*)
			enable_unaligned_access=no
			;;
	esac
fi
if test "x$enable_unaligned_access" = xyes ; then
	AC_DEFINE([HAVE_FAST_UNALIGNED_ACCESS], [1], [Define to 1 if
		the system supports fast unaligned memory access.])
	AC_MSG_RESULT([yes])
else
	AC_MSG_RESULT([no])
fi


#####################
# Size optimization #
#####################

AC_MSG_CHECKING([if small size is preferred over speed])
AC_ARG_ENABLE([small], AC_HELP_STRING([--enable-small],
		[Make liblzma smaller and a little slower.
		This is disabled by default to optimize for speed.]),
	[], [enable_small=no])
if test "x$enable_small" = xyes; then
	AC_DEFINE([HAVE_SMALL], [1], [Define to 1 if optimizing for size.])
elif test "x$enable_small" != xno; then
	AC_MSG_RESULT([])
	AC_MSG_ERROR([--enable-small accepts only \`yes' or \`no'])
fi
AC_MSG_RESULT([$enable_small])
AM_CONDITIONAL(COND_SMALL, test "x$enable_small" = xyes)


#############
# Threading #
#############

AC_MSG_CHECKING([if threading support is wanted])
AC_ARG_ENABLE([threads], AC_HELP_STRING([--disable-threads],
		[Disable threading support.
		This makes some things thread-unsafe.]),
	[], [enable_threads=yes])
if test "x$enable_threads" != xyes && test "x$enable_threads" != xno; then
	AC_MSG_RESULT([])
	AC_MSG_ERROR([--enable-threads accepts only \`yes' or \`no'])
fi
AC_MSG_RESULT([$enable_threads])
# We use the actual result a little later.


############################################
# xz/xzdec/lzmadec linkage against liblzma #
############################################

# Link the xz, xzdec, and lzmadec command line tools against static liblzma
# unless using --enable-dynamic. Using static liblzma gives a little bit
# faster executable on x86, because no register is wasted for PIC. We also
# have one dependency less, which allows users to more freely copy the xz
# binary to other boxes. However, I wouldn't be surprised if distro
# maintainers still prefer dynamic linking, so let's make it easy for them.

AC_MSG_CHECKING([how programs should be linked against liblzma])
AC_ARG_ENABLE([dynamic], [AC_HELP_STRING([--enable-dynamic=TYPE],
			[Set how command line tools are linked against liblzma.
			TYPE can be mixed, yes, or no. The default is mixed.])],
		[], [enable_dynamic=mixed])
case $enable_dynamic in
	mixed)
		AC_MSG_RESULT([mixed (some dynamically, some statically)])
		;;
	yes)
		AC_MSG_RESULT([dynamically])
		;;
	no)
		AC_MSG_RESULT([statically])
		;;
	*)
		AC_MSG_RESULT([])
		AC_MSG_ERROR([--enable-dynamic accepts only \`mixed', \`yes', or \`no'])
		;;
esac
# We use the actual results later, because we don't know yet
# if --disable-shared or --disable-static was used.


###############################################################################
# Checks for programs.
###############################################################################

echo
gl_POSIX_SHELL
if test -z "$POSIX_SHELL" ; then
	AC_MSG_ERROR([No POSIX conforming shell (sh) was found.])
fi

echo
echo "Initializing Automake:"

AM_INIT_AUTOMAKE([1.10 foreign tar-v7 filename-length-max=99])
AC_PROG_LN_S

AC_PROG_CC_C99
if test x$ac_cv_prog_cc_c99 = xno ; then
	AC_MSG_ERROR([No C99 compiler was found.])
fi

AM_PROG_CC_C_O
AM_PROG_AS
AC_USE_SYSTEM_EXTENSIONS

if test "x$enable_threads" = xyes; then
	echo
	echo "Threading support:"
	ACX_PTHREAD
	LIBS="$LIBS $PTHREAD_LIBS"
	AM_CFLAGS="$AM_CFLAGS $PTHREAD_CFLAGS"
	CC="$PTHREAD_CC"
fi

echo
echo "Initializing Libtool:"
LT_PREREQ([2.2])
LT_INIT([win32-dll])
LT_LANG([Windows Resource])

# This is a bit wrong since it is possible to request that only some libs
# are built as shared. Using that feature isn't so common though, and this
# breaks only on Windows (at least for now) if the user enables only some
# libs as shared.
AM_CONDITIONAL([COND_SHARED], [test "x$enable_shared" != xno])


###############################################################################
# Checks for libraries.
###############################################################################

echo
echo "Initializing gettext:"
AM_GNU_GETTEXT_VERSION([0.16.1])
AM_GNU_GETTEXT([external])

###############################################################################
# Checks for header files.
###############################################################################

echo
echo "System headers and functions:"

# There is currently no workarounds in this package if some of
# these headers are missing.
AC_CHECK_HEADERS([fcntl.h limits.h sys/time.h],
	[],
	[AC_MSG_ERROR([Required header file(s) are missing.])])

# If any of these headers are missing, things should still work correctly:
AC_CHECK_HEADERS([sys/param.h sys/sysctl.h byteswap.h],
	[], [], [
#ifdef HAVE_SYS_PARAM_H
#	include <sys/param.h>
#endif
])

# Even if we have byteswap.h, we may lack the specific macros/functions.
if test x$ac_cv_header_byteswap_h = xyes ; then
	m4_foreach([FUNC], [bswap_16,bswap_32,bswap_64], [
		AC_MSG_CHECKING([if FUNC is available])
		AC_LINK_IFELSE([AC_LANG_SOURCE([
#include <byteswap.h>
int
main(void)
{
	FUNC[](42);
	return 0;
}
		])], [
			AC_DEFINE(HAVE_[]m4_toupper(FUNC), [1],
					[Define to 1 if] FUNC [is available.])
			AC_MSG_RESULT([yes])
		], [AC_MSG_RESULT([no])])

	])dnl
fi


###############################################################################
# Checks for typedefs, structures, and compiler characteristics.
###############################################################################

dnl We don't need these as long as we need a C99 compiler anyway.
dnl AC_C_INLINE
dnl AC_C_RESTRICT

AC_HEADER_STDBOOL

AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_INT32_T
AC_TYPE_UINT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT64_T
AC_TYPE_UINTPTR_T

AC_CHECK_SIZEOF([size_t])

# The command line tool can copy high resolution timestamps if such
# information is availabe in struct stat. Otherwise one second accuracy
# is used.
AC_CHECK_MEMBERS([
	struct stat.st_atim.tv_nsec,
	struct stat.st_atimespec.tv_nsec,
	struct stat.st_atimensec,
	struct stat.st_uatime,
	struct stat.st_atim.st__tim.tv_nsec])

AC_SYS_LARGEFILE
AC_C_BIGENDIAN


###############################################################################
# Checks for library functions.
###############################################################################

# Gnulib replacements as needed
gl_GETOPT

# Find the best function to set timestamps.
AC_CHECK_FUNCS([futimens futimes futimesat utimes utime], [break])

lc_PHYSMEM
lc_CPUCORES


###############################################################################
# If using GCC, set some additional AM_CFLAGS:
###############################################################################

if test "$GCC" = yes ; then
	echo
	echo "GCC extensions:"
fi

# Always do the visibility check but don't set AM_CFLAGS on Windows.
# This way things get set properly even on Windows.
gl_VISIBILITY
if test -n "$CFLAG_VISIBILITY" && test "$is_w32" = no; then
	AM_CFLAGS="$AM_CFLAGS $CFLAG_VISIBILITY"
fi

if test "$GCC" = yes ; then
	# Enable as much warnings as possible. These commented warnings won't
	# work for this package though:
	#   * -Wunreachable-code breaks several assert(0) cases, which are
	#     backed up with "return LZMA_PROG_ERROR".
	#   * -Wcast-qual would break various things where we need a non-const
	#     pointer although we don't modify anything through it.
	#   * -Wcast-align breaks optimized CRC32 and CRC64 implementation
	#     on some architectures (not on x86), where this warning is bogus,
	#     because we take care of correct alignment.
	#   * -Winline, -Wdisabled-optimization, -Wunsafe-loop-optimizations
	#     don't seem so useful here; at least the last one gives some
	#     warnings which are not bugs.
	for NEW_FLAG in \
			-Wall \
			-Wextra \
			-Wformat=2 \
			-Winit-self \
			-Wmissing-include-dirs \
			-Wstrict-aliasing \
			-Wfloat-equal \
			-Wundef \
			-Wshadow \
			-Wpointer-arith \
			-Wbad-function-cast \
			-Wwrite-strings \
			-Wlogical-op \
			-Waggregate-return \
			-Wstrict-prototypes \
			-Wold-style-definition \
			-Wmissing-prototypes \
			-Wmissing-declarations \
			-Wmissing-noreturn \
			-Wredundant-decls
	do
		AC_MSG_CHECKING([if $CC accepts $NEW_FLAG])
		OLD_CFLAGS="$CFLAGS"
		CFLAGS="$CFLAGS $NEW_FLAG"
		AC_COMPILE_IFELSE([void foo(void) { }], [
			AM_CFLAGS="$AM_CFLAGS $NEW_FLAG"
			AC_MSG_RESULT([yes])
		], [
			AC_MSG_RESULT([no])
		])
		CFLAGS="$OLD_CFLAGS"
	done

	AC_ARG_ENABLE([werror],
		AC_HELP_STRING([--enable-werror], [Enable -Werror to abort
			compilation on all compiler warnings.]),
		[], [enable_werror=no])
	if test "x$enable_werror" = "xyes"; then
		AM_CFLAGS="$AM_CFLAGS -Werror"
	fi
fi


###############################################################################
# Create the makefiles and config.h
###############################################################################

echo

# Don't build the lib directory at all if we don't need any replacement
# functions.
AM_CONDITIONAL([COND_GNULIB], test -n "$LIBOBJS")

# Add default AM_CFLAGS.
AC_SUBST([AM_CFLAGS])

# Set additional flags for static/dynamic linking. The idea is that every
# program (not library) being built will use either STATIC_{CPPFLAGS,LDFLAGS}
# or DYNAMIC_{CPPFLAGS,LDFLAGS} depending on which type of linkage is
# preferred. These preferences get overriden by use of --disable-static,
# --disable-shared, or --enable-dynamic.
#
# This is quite messy, because we want to use LZMA_API_STATIC when linking
# against static liblzma. It's needed on Windows.
if test "x$enable_static" = xno; then
	enable_dynamic=yes
fi
if test "x$enable_shared" = xno; then
	enable_dynamic=no
fi
case $enable_dynamic in
	yes)
		STATIC_CPPFLAGS=
		STATIC_LDFLAGS=
		DYNAMIC_CPPFLAGS=
		DYNAMIC_LDFLAGS=
		;;
	mixed)
		STATIC_CPPFLAGS="-DLZMA_API_STATIC"
		STATIC_LDFLAGS="-static"
		DYNAMIC_CPPFLAGS=
		DYNAMIC_LDFLAGS=
		;;
	no)
		STATIC_CPPFLAGS="-DLZMA_API_STATIC"
		STATIC_LDFLAGS="-static"
		DYNAMIC_CPPFLAGS="-DLZMA_API_STATIC"
		DYNAMIC_LDFLAGS="-static"
		;;
esac
AC_SUBST([STATIC_CPPFLAGS])
AC_SUBST([STATIC_LDFLAGS])
AC_SUBST([DYNAMIC_CPPFLAGS])
AC_SUBST([DYNAMIC_LDFLAGS])

# This is needed for src/scripts.
xz=`echo xz | sed "$program_transform_name"`
AC_SUBST([xz])

AC_CONFIG_FILES([
	Doxyfile
	Makefile
	po/Makefile.in
	lib/Makefile
	src/Makefile
	src/liblzma/liblzma.pc
	src/liblzma/Makefile
	src/liblzma/api/Makefile
	src/xz/Makefile
	src/xzdec/Makefile
	src/lzmainfo/Makefile
	src/scripts/Makefile
	src/scripts/xzdiff
	src/scripts/xzgrep
	src/scripts/xzmore
	src/scripts/xzless
	tests/Makefile
	debug/Makefile
])

AC_OUTPUT
