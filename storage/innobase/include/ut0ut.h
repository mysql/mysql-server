/*****************************************************************************

Copyright (c) 1994, 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/******************************************************************//**
@file include/ut0ut.h
Various utilities

Created 1/20/1994 Heikki Tuuri
***********************************************************************/

#ifndef ut0ut_h
#define ut0ut_h

#include "univ.i"

#ifndef UNIV_INNOCHECKSUM

#include "db0err.h"

#ifndef UNIV_HOTBACKUP
# include "os0sync.h" /* for HAVE_ATOMIC_BUILTINS */
#endif /* UNIV_HOTBACKUP */

#include <time.h>
#ifndef MYSQL_SERVER
#include <ctype.h>
#endif

#include <stdarg.h> /* for va_list */

/** Index name prefix in fast index creation */
#define	TEMP_INDEX_PREFIX	'\377'
/** Index name prefix in fast index creation, as a string constant */
#define TEMP_INDEX_PREFIX_STR	"\377"

/** Time stamp */
typedef time_t	ib_time_t;

/* In order to call a piece of code, when a function returns or when the
scope ends, use this utility class.  It will invoke the given function
object in its destructor. */
template<typename F>
struct ut_when_dtor {
	ut_when_dtor(F& p) : f(p) {}
	~ut_when_dtor() {
		f();
	}
private:
	F& f;
};

#ifndef UNIV_HOTBACKUP
# if defined(HAVE_PAUSE_INSTRUCTION)
   /* According to the gcc info page, asm volatile means that the
   instruction has important side-effects and must not be removed.
   Also asm volatile may trigger a memory barrier (spilling all registers
   to memory). */
#  ifdef __SUNPRO_CC
#   define UT_RELAX_CPU() asm ("pause" )
#  else
#   define UT_RELAX_CPU() __asm__ __volatile__ ("pause")
#  endif /* __SUNPRO_CC */

# elif defined(HAVE_FAKE_PAUSE_INSTRUCTION)
#  define UT_RELAX_CPU() __asm__ __volatile__ ("rep; nop")
# elif defined(HAVE_ATOMIC_BUILTINS)
#  define UT_RELAX_CPU() __asm__ __volatile__ ("":::"memory")
# elif defined(HAVE_WINDOWS_ATOMICS)
   /* In the Win32 API, the x86 PAUSE instruction is executed by calling
   the YieldProcessor macro defined in WinNT.h. It is a CPU architecture-
   independent way by using YieldProcessor. */
#  define UT_RELAX_CPU() YieldProcessor()
# else
#  define UT_RELAX_CPU() ((void)0) /* avoid warning for an empty statement */
# endif

# if defined(HAVE_HMT_PRIORITY_INSTRUCTION)
#  define UT_LOW_PRIORITY_CPU() __asm__ __volatile__ ("or 1,1,1")
#  define UT_RESUME_PRIORITY_CPU() __asm__ __volatile__ ("or 2,2,2")
# else
#  define UT_LOW_PRIORITY_CPU() ((void)0)
#  define UT_RESUME_PRIORITY_CPU() ((void)0)
# endif

/*********************************************************************//**
Delays execution for at most max_wait_us microseconds or returns earlier
if cond becomes true.
@param cond		in: condition to wait for; evaluated every 2 ms
@param max_wait_us	in: maximum delay to wait, in microseconds */
#define UT_WAIT_FOR(cond, max_wait_us)				\
do {								\
	ullint	start_us;					\
	start_us = ut_time_us(NULL);				\
	while (!(cond) 						\
	       && ut_time_us(NULL) - start_us < (max_wait_us)) {\
								\
		os_thread_sleep(2000 /* 2 ms */);		\
	}							\
} while (0)
#endif /* !UNIV_HOTBACKUP */

template <class T> T ut_min(T a, T b) { return(a < b ? a : b); }
template <class T> T ut_max(T a, T b) { return(a > b ? a : b); }

/******************************************************//**
Calculates the minimum of two ulints.
@return	minimum */
UNIV_INLINE
ulint
ut_min(
/*===*/
	ulint	 n1,	/*!< in: first number */
	ulint	 n2);	/*!< in: second number */
/******************************************************//**
Calculates the maximum of two ulints.
@return	maximum */
UNIV_INLINE
ulint
ut_max(
/*===*/
	ulint	 n1,	/*!< in: first number */
	ulint	 n2);	/*!< in: second number */
/****************************************************************//**
Calculates minimum of two ulint-pairs. */
UNIV_INLINE
void
ut_pair_min(
/*========*/
	ulint*	a,	/*!< out: more significant part of minimum */
	ulint*	b,	/*!< out: less significant part of minimum */
	ulint	a1,	/*!< in: more significant part of first pair */
	ulint	b1,	/*!< in: less significant part of first pair */
	ulint	a2,	/*!< in: more significant part of second pair */
	ulint	b2);	/*!< in: less significant part of second pair */
/******************************************************//**
Compares two ulints.
@return	1 if a > b, 0 if a == b, -1 if a < b */
UNIV_INLINE
int
ut_ulint_cmp(
/*=========*/
	ulint	a,	/*!< in: ulint */
	ulint	b);	/*!< in: ulint */
/*******************************************************//**
Compares two pairs of ulints.
@return	-1 if a < b, 0 if a == b, 1 if a > b */
UNIV_INLINE
int
ut_pair_cmp(
/*========*/
	ulint	a1,	/*!< in: more significant part of first pair */
	ulint	a2,	/*!< in: less significant part of first pair */
	ulint	b1,	/*!< in: more significant part of second pair */
	ulint	b2);	/*!< in: less significant part of second pair */
/*************************************************************//**
Determines if a number is zero or a power of two.
@param n	in: number
@return		nonzero if n is zero or a power of two; zero otherwise */
#define ut_is_2pow(n) UNIV_LIKELY(!((n) & ((n) - 1)))
/*************************************************************//**
Calculates fast the remainder of n/m when m is a power of two.
@param n	in: numerator
@param m	in: denominator, must be a power of two
@return		the remainder of n/m */
#define ut_2pow_remainder(n, m) ((n) & ((m) - 1))
/*************************************************************//**
Calculates the biggest multiple of m that is not bigger than n
when m is a power of two.  In other words, rounds n down to m * k.
@param n	in: number to round down
@param m	in: alignment, must be a power of two
@return		n rounded down to the biggest possible integer multiple of m */
#define ut_2pow_round(n, m) ((n) & ~((m) - 1))
/** Align a number down to a multiple of a power of two.
@param n	in: number to round down
@param m	in: alignment, must be a power of two
@return		n rounded down to the biggest possible integer multiple of m */
#define ut_calc_align_down(n, m) ut_2pow_round(n, m)
/********************************************************//**
Calculates the smallest multiple of m that is not smaller than n
when m is a power of two.  In other words, rounds n up to m * k.
@param n	in: number to round up
@param m	in: alignment, must be a power of two
@return		n rounded up to the smallest possible integer multiple of m */
#define ut_calc_align(n, m) (((n) + ((m) - 1)) & ~((m) - 1))
/*************************************************************//**
Calculates fast the 2-logarithm of a number, rounded upward to an
integer.
@return	logarithm in the base 2, rounded upward */
UNIV_INLINE
ulint
ut_2_log(
/*=====*/
	ulint	n);	/*!< in: number */
/*************************************************************//**
Calculates 2 to power n.
@return	2 to power n */
UNIV_INLINE
ulint
ut_2_exp(
/*=====*/
	ulint	n);	/*!< in: number */
/*************************************************************//**
Calculates fast the number rounded up to the nearest power of 2.
@return	first power of 2 which is >= n */
UNIV_INTERN
ulint
ut_2_power_up(
/*==========*/
	ulint	n)	/*!< in: number != 0 */
	MY_ATTRIBUTE((const));

/** Determine how many bytes (groups of 8 bits) are needed to
store the given number of bits.
@param b	in: bits
@return		number of bytes (octets) needed to represent b */
#define UT_BITS_IN_BYTES(b) (((b) + 7) / 8)

/**********************************************************//**
Returns system time. We do not specify the format of the time returned:
the only way to manipulate it is to use the function ut_difftime.
@return	system time */
UNIV_INTERN
ib_time_t
ut_time(void);
/*=========*/
#ifndef UNIV_HOTBACKUP
/**********************************************************//**
Returns system time.
Upon successful completion, the value 0 is returned; otherwise the
value -1 is returned and the global variable errno is set to indicate the
error.
@return	0 on success, -1 otherwise */
UNIV_INTERN
int
ut_usectime(
/*========*/
	ulint*	sec,	/*!< out: seconds since the Epoch */
	ulint*	ms);	/*!< out: microseconds since the Epoch+*sec */

/**********************************************************//**
Returns the number of microseconds since epoch. Similar to
time(3), the return value is also stored in *tloc, provided
that tloc is non-NULL.
@return	us since epoch */
UNIV_INTERN
ullint
ut_time_us(
/*=======*/
	ullint*	tloc);	/*!< out: us since epoch, if non-NULL */
/**********************************************************//**
Returns the number of milliseconds since some epoch.  The
value may wrap around.  It should only be used for heuristic
purposes.
@return	ms since epoch */
UNIV_INTERN
ulint
ut_time_ms(void);
/*============*/
#endif /* !UNIV_HOTBACKUP */

/**********************************************************//**
Returns the number of milliseconds since some epoch.  The
value may wrap around.  It should only be used for heuristic
purposes.
@return ms since epoch */
UNIV_INTERN
ulint
ut_time_ms(void);
/*============*/

/**********************************************************//**
Returns the difference of two times in seconds.
@return	time2 - time1 expressed in seconds */
UNIV_INTERN
double
ut_difftime(
/*========*/
	ib_time_t	time2,	/*!< in: time */
	ib_time_t	time1);	/*!< in: time */

#endif /* !UNIV_INNOCHECKSUM */

/**********************************************************//**
Prints a timestamp to a file. */
UNIV_INTERN
void
ut_print_timestamp(
/*===============*/
	FILE*	file)	/*!< in: file where to print */
	UNIV_COLD MY_ATTRIBUTE((nonnull));

#ifndef UNIV_INNOCHECKSUM

/**********************************************************//**
Sprintfs a timestamp to a buffer, 13..14 chars plus terminating NUL. */
UNIV_INTERN
void
ut_sprintf_timestamp(
/*=================*/
	char*	buf); /*!< in: buffer where to sprintf */
#ifdef UNIV_HOTBACKUP
/**********************************************************//**
Sprintfs a timestamp to a buffer with no spaces and with ':' characters
replaced by '_'. */
UNIV_INTERN
void
ut_sprintf_timestamp_without_extra_chars(
/*=====================================*/
	char*	buf); /*!< in: buffer where to sprintf */
/**********************************************************//**
Returns current year, month, day. */
UNIV_INTERN
void
ut_get_year_month_day(
/*==================*/
	ulint*	year,	/*!< out: current year */
	ulint*	month,	/*!< out: month */
	ulint*	day);	/*!< out: day */
#else /* UNIV_HOTBACKUP */
/*************************************************************//**
Runs an idle loop on CPU. The argument gives the desired delay
in microseconds on 100 MHz Pentium + Visual C++.
@return	dummy value */
UNIV_INTERN
ulint
ut_delay(
/*=====*/
	ulint	delay);	/*!< in: delay in microseconds on 100 MHz Pentium */
#endif /* UNIV_HOTBACKUP */
/*************************************************************//**
Prints the contents of a memory buffer in hex and ascii. */
UNIV_INTERN
void
ut_print_buf(
/*=========*/
	FILE*		file,	/*!< in: file where to print */
	const void*	buf,	/*!< in: memory buffer */
	ulint		len);	/*!< in: length of the buffer */

/**********************************************************************//**
Outputs a NUL-terminated file name, quoted with apostrophes. */
UNIV_INTERN
void
ut_print_filename(
/*==============*/
	FILE*		f,	/*!< in: output stream */
	const char*	name);	/*!< in: name to print */

#ifndef UNIV_HOTBACKUP
/* Forward declaration of transaction handle */
struct trx_t;

/**********************************************************************//**
Outputs a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier. */
UNIV_INTERN
void
ut_print_name(
/*==========*/
	FILE*		f,	/*!< in: output stream */
	const trx_t*	trx,	/*!< in: transaction */
	ibool		table_id,/*!< in: TRUE=print a table name,
				FALSE=print other identifier */
	const char*	name);	/*!< in: name to print */

/**********************************************************************//**
Outputs a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier. */
UNIV_INTERN
void
ut_print_namel(
/*===========*/
	FILE*		f,	/*!< in: output stream */
	const trx_t*	trx,	/*!< in: transaction (NULL=no quotes) */
	ibool		table_id,/*!< in: TRUE=print a table name,
				FALSE=print other identifier */
	const char*	name,	/*!< in: name to print */
	ulint		namelen);/*!< in: length of name */

/**********************************************************************//**
Formats a table or index name, quoted as an SQL identifier. If the name
contains a slash '/', the result will contain two identifiers separated by
a period (.), as in SQL database_name.identifier.
@return pointer to 'formatted' */
UNIV_INTERN
char*
ut_format_name(
/*===========*/
	const char*	name,		/*!< in: table or index name, must be
					'\0'-terminated */
	ibool		is_table,	/*!< in: if TRUE then 'name' is a table
					name */
	char*		formatted,	/*!< out: formatted result, will be
					'\0'-terminated */
	ulint		formatted_size);/*!< out: no more than this number of
					bytes will be written to 'formatted' */

/**********************************************************************//**
Catenate files. */
UNIV_INTERN
void
ut_copy_file(
/*=========*/
	FILE*	dest,	/*!< in: output file */
	FILE*	src);	/*!< in: input file to be appended to output */
#endif /* !UNIV_HOTBACKUP */

#ifdef __WIN__
/**********************************************************************//**
A substitute for vsnprintf(3), formatted output conversion into
a limited buffer. Note: this function DOES NOT return the number of
characters that would have been printed if the buffer was unlimited because
VC's _vsnprintf() returns -1 in this case and we would need to call
_vscprintf() in addition to estimate that but we would need another copy
of "ap" for that and VC does not provide va_copy(). */
UNIV_INTERN
void
ut_vsnprintf(
/*=========*/
	char*		str,	/*!< out: string */
	size_t		size,	/*!< in: str size */
	const char*	fmt,	/*!< in: format */
	va_list		ap);	/*!< in: format values */

/**********************************************************************//**
A substitute for snprintf(3), formatted output conversion into
a limited buffer.
@return number of characters that would have been printed if the size
were unlimited, not including the terminating '\0'. */
UNIV_INTERN
int
ut_snprintf(
/*========*/
	char*		str,	/*!< out: string */
	size_t		size,	/*!< in: str size */
	const char*	fmt,	/*!< in: format */
	...);			/*!< in: format values */
#else
/**********************************************************************//**
A wrapper for vsnprintf(3), formatted output conversion into
a limited buffer. Note: this function DOES NOT return the number of
characters that would have been printed if the buffer was unlimited because
VC's _vsnprintf() returns -1 in this case and we would need to call
_vscprintf() in addition to estimate that but we would need another copy
of "ap" for that and VC does not provide va_copy(). */
# define ut_vsnprintf(buf, size, fmt, ap)	\
	((void) vsnprintf(buf, size, fmt, ap))
/**********************************************************************//**
A wrapper for snprintf(3), formatted output conversion into
a limited buffer. */
# define ut_snprintf	snprintf
#endif /* __WIN__ */

/*************************************************************//**
Convert an error number to a human readable text message. The
returned string is static and should not be freed or modified.
@return	string, describing the error */
UNIV_INTERN
const char*
ut_strerr(
/*======*/
	dberr_t	num);	/*!< in: error number */

/****************************************************************
Sort function for ulint arrays. */
UNIV_INTERN
void
ut_ulint_sort(
/*==========*/
	ulint*	arr,		/*!< in/out: array to sort */
	ulint*	aux_arr,	/*!< in/out: aux array to use in sort */
	ulint	low,		/*!< in: lower bound */
	ulint	high)		/*!< in: upper bound */
	MY_ATTRIBUTE((nonnull));

#ifndef UNIV_NONINL
#include "ut0ut.ic"
#endif

#endif /* !UNIV_INNOCHECKSUM */

#endif

