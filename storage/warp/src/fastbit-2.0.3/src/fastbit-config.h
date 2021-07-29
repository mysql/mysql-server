/* src/fastbit-config.h.  Generated from fastbit-config.h.in by configure.  */
/* src/fastbit-config.h.in.  Generated from configure.ac by autoheader.  */

/* define the null values used */
#define FASTBIT_DOUBLE_NULL std::numeric_limits<double>::quiet_NaN()

/* define the null values used */
#define FASTBIT_FLOAT_NULL std::numeric_limits<float>::quiet_NaN()

/* Define an integer version of FastBit IBIS version number */
#define FASTBIT_IBIS_INT_VERSION 0x0000300

/* Compiler support <atomic> template */
#define HAVE_ATOMIC_TEMPLATE 1

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you don't have `vprintf' but do have `_doprnt.' */
/* #undef HAVE_DOPRNT */

/* Define to 1 if you have the `flock' function. */
#define HAVE_FLOCK 1

/* Define to 1 if support __sync_add_and_fetch() on 32-bit integers */
#define HAVE_GCC_ATOMIC32 1

/* Define to 1 if support __sync_add_and_fetch() on 64-bit integers */
#define HAVE_GCC_ATOMIC64 1

/* Set to 1 if getpwuid compiles and runs correctly */
#define HAVE_GETPWUID 1

/* Set to 1 if getpwuid_r compiles and runs correctly */
#define HAVE_GETPWUID_R 1

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the `rt' library (-lrt). */
#define HAVE_LIBRT 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkdir' function. */
#define HAVE_MKDIR 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if nextafter is defined in math.h and is in libm. */
#define HAVE_NEXTAFTER 1

/* Define if you have POSIX threads libraries and header files. */
#define HAVE_PTHREAD 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strptime' function. */
#define HAVE_STRPTIME 1

/* Set to 1 if strtoll is defined in stdlib.h */
#define HAVE_STRTOLL 1

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Compiler support std::unordered_map */
#define HAVE_UNORDERED_MAP 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Define to the address where bug reports for this package should be sent. */
#define FASTBIT_BUGREPORT "fastbit-users@hpcrdm.lbl.gov"

/* Define to the full name of this package. */
#define FASTBIT_NAME "FastBit"

/* Define to the full name and version of this package. */
#define FASTBIT_STRING "FastBit 2.0.3"

/* Define to the one symbol short name of this package. */
#define FASTBIT_TARNAME "fastbit"

/* Define to the home page for this package. */
#define FASTBIT_URL ""

/* Define to the version of this package. */
#define FASTBIT_VERSION "2.0.3"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1
