/* Function declarations for libiberty.
   Written by Cygnus Support, 1994.

   The libiberty library provides a number of functions which are
   missing on some operating systems.  We do not declare those here,
   to avoid conflicts with the system header files on operating
   systems that do support those functions.  In this file we only
   declare those functions which are specific to libiberty.  */

#ifndef LIBIBERTY_H
#define LIBIBERTY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ansidecl.h"

/* Build an argument vector from a string.  Allocates memory using
   malloc.  Use freeargv to free the vector.  */

extern char **buildargv PARAMS ((char *));

/* Free a vector returned by buildargv.  */

extern void freeargv PARAMS ((char **));

/* Duplicate an argument vector. Allocates memory using malloc.  Use
   freeargv to free the vector.  */

extern char **dupargv PARAMS ((char **));


/* Return the last component of a path name.  Note that we can't use a
   prototype here because the parameter is declared inconsistently
   across different systems, sometimes as "char *" and sometimes as
   "const char *" */

#if defined (__GNU_LIBRARY__ ) || defined (__linux__) || defined (__FreeBSD__)
extern char *basename PARAMS ((const char *));
#else
extern char *basename ();
#endif

/* Concatenate an arbitrary number of strings, up to (char *) NULL.
   Allocates memory using xmalloc.  */

extern char *concat PARAMS ((const char *, ...));

/* Check whether two file descriptors refer to the same file.  */

extern int fdmatch PARAMS ((int fd1, int fd2));

/* Get the amount of time the process has run, in microseconds.  */

extern long get_run_time PARAMS ((void));

/* Choose a temporary directory to use for scratch files.  */

extern char *choose_temp_base PARAMS ((void));

/* Allocate memory filled with spaces.  Allocates using malloc.  */

extern const char *spaces PARAMS ((int count));

/* Return the maximum error number for which strerror will return a
   string.  */

extern int errno_max PARAMS ((void));

/* Return the name of an errno value (e.g., strerrno (EINVAL) returns
   "EINVAL").  */

extern const char *strerrno PARAMS ((int));

/* Given the name of an errno value, return the value.  */

extern int strtoerrno PARAMS ((const char *));

/* ANSI's strerror(), but more robust.  */

extern char *xstrerror PARAMS ((int));

/* Return the maximum signal number for which strsignal will return a
   string.  */

extern int signo_max PARAMS ((void));

/* Return a signal message string for a signal number
   (e.g., strsignal (SIGHUP) returns something like "Hangup").  */
/* This is commented out as it can conflict with one in system headers.
   We still document its existence though.  */

/*extern const char *strsignal PARAMS ((int));*/

/* Return the name of a signal number (e.g., strsigno (SIGHUP) returns
   "SIGHUP").  */

extern const char *strsigno PARAMS ((int));

/* Given the name of a signal, return its number.  */

extern int strtosigno PARAMS ((const char *));

/* Register a function to be run by xexit.  Returns 0 on success.  */

extern int xatexit PARAMS ((void (*fn) (void)));

/* Exit, calling all the functions registered with xatexit.  */

#ifndef __GNUC__
extern void xexit PARAMS ((int status));
#else
void xexit PARAMS ((int status)) __attribute__ ((noreturn));
#endif

/* Set the program name used by xmalloc.  */

extern void xmalloc_set_program_name PARAMS ((const char *));

/* Allocate memory without fail.  If malloc fails, this will print a
   message to stderr (using the name set by xmalloc_set_program_name,
   if any) and then call xexit.  */

#ifdef ANSI_PROTOTYPES
/* Get a definition for size_t.  */
#include <stddef.h>
#endif
extern PTR xmalloc PARAMS ((size_t));

/* Reallocate memory without fail.  This works like xmalloc.

   FIXME: We do not declare the parameter types for the same reason as
   xmalloc.  */

extern PTR xrealloc PARAMS ((PTR, size_t));

/* Allocate memory without fail and set it to zero.  This works like
   xmalloc.  */

extern PTR xcalloc PARAMS ((size_t, size_t));

/* Copy a string into a memory buffer without fail.  */

extern char *xstrdup PARAMS ((const char *));

/* hex character manipulation routines */

#define _hex_array_size 256
#define _hex_bad	99
extern char _hex_value[_hex_array_size];
extern void hex_init PARAMS ((void));
#define hex_p(c)	(hex_value (c) != _hex_bad)
/* If you change this, note well: Some code relies on side effects in
   the argument being performed exactly once.  */
#define hex_value(c)	(_hex_value[(unsigned char) (c)])

/* Definitions used by the pexecute routine.  */

#define PEXECUTE_FIRST   1
#define PEXECUTE_LAST    2
#define PEXECUTE_ONE     (PEXECUTE_FIRST + PEXECUTE_LAST)
#define PEXECUTE_SEARCH  4
#define PEXECUTE_VERBOSE 8

/* Execute a program.  */

extern int pexecute PARAMS ((const char *, char * const *, const char *,
			    const char *, char **, char **, int));

/* Wait for pexecute to finish.  */

extern int pwait PARAMS ((int, int *, int));

#ifdef __cplusplus
}
#endif


#endif /* ! defined (LIBIBERTY_H) */
