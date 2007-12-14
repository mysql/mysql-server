/**********************************************************************
Various utilities

(c) 1994, 1995 Innobase Oy

Created 1/20/1994 Heikki Tuuri
***********************************************************************/

#ifndef ut0ut_h
#define ut0ut_h

#include "univ.i"
#include <time.h>
#ifndef MYSQL_SERVER
#include <ctype.h>
#endif

typedef time_t	ib_time_t;

/************************************************************
Gets the high 32 bits in a ulint. That is makes a shift >> 32,
but since there seem to be compiler bugs in both gcc and Visual C++,
we do this by a special conversion. */

ulint
ut_get_high32(
/*==========*/
			/* out: a >> 32 */
	ulint	a);	/* in: ulint */
/**********************************************************
Calculates the minimum of two ulints. */
UNIV_INLINE
ulint
ut_min(
/*===*/
			/* out: minimum */
	ulint	 n1,	/* in: first number */
	ulint	 n2);	/* in: second number */
/**********************************************************
Calculates the maximum of two ulints. */
UNIV_INLINE
ulint
ut_max(
/*===*/
			/* out: maximum */
	ulint	 n1,	/* in: first number */
	ulint	 n2);	/* in: second number */
/********************************************************************
Calculates minimum of two ulint-pairs. */
UNIV_INLINE
void
ut_pair_min(
/*========*/
	ulint*	a,	/* out: more significant part of minimum */
	ulint*	b,	/* out: less significant part of minimum */
	ulint	a1,	/* in: more significant part of first pair */
	ulint	b1,	/* in: less significant part of first pair */
	ulint	a2,	/* in: more significant part of second pair */
	ulint	b2);	/* in: less significant part of second pair */
/**********************************************************
Compares two ulints. */
UNIV_INLINE
int
ut_ulint_cmp(
/*=========*/
			/* out: 1 if a > b, 0 if a == b, -1 if a < b */
	ulint	a,	/* in: ulint */
	ulint	b);	/* in: ulint */
/***********************************************************
Compares two pairs of ulints. */
UNIV_INLINE
int
ut_pair_cmp(
/*========*/
			/* out: -1 if a < b, 0 if a == b,
			1 if a > b */
	ulint	a1,	/* in: more significant part of first pair */
	ulint	a2,	/* in: less significant part of first pair */
	ulint	b1,	/* in: more significant part of second pair */
	ulint	b2);	/* in: less significant part of second pair */
/*****************************************************************
Calculates fast the remainder when divided by a power of two. */
UNIV_INLINE
ulint
ut_2pow_remainder(
/*==============*/	/* out: remainder */
	ulint	n,	/* in: number to be divided */
	ulint	m);	/* in: divisor; power of 2 */
/*****************************************************************
Calculates fast value rounded to a multiple of a power of 2. */
UNIV_INLINE
ulint
ut_2pow_round(
/*==========*/		/* out: value of n rounded down to nearest
			multiple of m */
	ulint	n,	/* in: number to be rounded */
	ulint	m);	/* in: divisor; power of 2 */
/*****************************************************************
Calculates fast the 2-logarithm of a number, rounded upward to an
integer. */
UNIV_INLINE
ulint
ut_2_log(
/*=====*/
			/* out: logarithm in the base 2, rounded upward */
	ulint	n);	/* in: number */
/*****************************************************************
Calculates 2 to power n. */
UNIV_INLINE
ulint
ut_2_exp(
/*=====*/
			/* out: 2 to power n */
	ulint	n);	/* in: number */
/*****************************************************************
Calculates fast the number rounded up to the nearest power of 2. */

ulint
ut_2_power_up(
/*==========*/
			/* out: first power of 2 which is >= n */
	ulint	n)	/* in: number != 0 */
	__attribute__((const));

/* Determine how many bytes (groups of 8 bits) are needed to
store the given number of bits. */
#define UT_BITS_IN_BYTES(b) (((b) + 7) / 8)

/****************************************************************
Sort function for ulint arrays. */

void
ut_ulint_sort(ulint* arr, ulint* aux_arr, ulint low, ulint high);
/*============================================================*/
/************************************************************
The following function returns elapsed CPU time in milliseconds. */

ulint
ut_clock(void);
/**************************************************************
Returns system time. We do not specify the format of the time returned:
the only way to manipulate it is to use the function ut_difftime. */

ib_time_t
ut_time(void);
/*=========*/
/**************************************************************
Returns system time. */

void
ut_usectime(
/*========*/
	ulint*	sec,	/* out: seconds since the Epoch */
	ulint*	ms);	/* out: microseconds since the Epoch+*sec */
/**************************************************************
Returns the difference of two times in seconds. */

double
ut_difftime(
/*========*/
				/* out: time2 - time1 expressed in seconds */
	ib_time_t	time2,	/* in: time */
	ib_time_t	time1);	/* in: time */
/**************************************************************
Prints a timestamp to a file. */

void
ut_print_timestamp(
/*===============*/
	FILE*  file); /* in: file where to print */
/**************************************************************
Sprintfs a timestamp to a buffer, 13..14 chars plus terminating NUL. */

void
ut_sprintf_timestamp(
/*=================*/
	char*	buf); /* in: buffer where to sprintf */
/**************************************************************
Sprintfs a timestamp to a buffer with no spaces and with ':' characters
replaced by '_'. */

void
ut_sprintf_timestamp_without_extra_chars(
/*=====================================*/
	char*	buf); /* in: buffer where to sprintf */
/**************************************************************
Returns current year, month, day. */

void
ut_get_year_month_day(
/*==================*/
	ulint*	year,	/* out: current year */
	ulint*	month,	/* out: month */
	ulint*	day);	/* out: day */
/*****************************************************************
Runs an idle loop on CPU. The argument gives the desired delay
in microseconds on 100 MHz Pentium + Visual C++. */

ulint
ut_delay(
/*=====*/
			/* out: dummy value */
	ulint	delay);	/* in: delay in microseconds on 100 MHz Pentium */
/*****************************************************************
Prints the contents of a memory buffer in hex and ascii. */

void
ut_print_buf(
/*=========*/
	FILE*		file,	/* in: file where to print */
	const void*	buf,	/* in: memory buffer */
	ulint		len);	/* in: length of the buffer */

/**************************************************************************
Outputs a NUL-terminated file name, quoted with apostrophes. */

void
ut_print_filename(
/*==============*/
	FILE*		f,	/* in: output stream */
	const char*	name);	/* in: name to print */

/* Forward declaration of transaction handle */
struct trx_struct;

/**************************************************************************
Outputs a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier. */

void
ut_print_name(
/*==========*/
	FILE*		f,	/* in: output stream */
	struct trx_struct*trx,	/* in: transaction */
	ibool		table_id,/* in: TRUE=print a table name,
				FALSE=print other identifier */
	const char*	name);	/* in: name to print */

/**************************************************************************
Outputs a fixed-length string, quoted as an SQL identifier.
If the string contains a slash '/', the string will be
output as two identifiers separated by a period (.),
as in SQL database_name.identifier. */

void
ut_print_namel(
/*===========*/
	FILE*		f,	/* in: output stream */
	struct trx_struct*trx,	/* in: transaction (NULL=no quotes) */
	ibool		table_id,/* in: TRUE=print a table name,
				FALSE=print other identifier */
	const char*	name,	/* in: name to print */
	ulint		namelen);/* in: length of name */

/**************************************************************************
Catenate files. */

void
ut_copy_file(
/*=========*/
	FILE*	dest,	/* in: output file */
	FILE*	src);	/* in: input file to be appended to output */

/**************************************************************************
snprintf(). */

#ifdef __WIN__
int
ut_snprintf(
				/* out: number of characters that would
				have been printed if the size were
				unlimited, not including the terminating
				'\0'. */
	char*		str,	/* out: string */
	size_t		size,	/* in: str size */
	const char*	fmt,	/* in: format */
	...);			/* in: format values */
#else
#define ut_snprintf	snprintf
#endif /* __WIN__ */

#ifndef UNIV_NONINL
#include "ut0ut.ic"
#endif

#endif

