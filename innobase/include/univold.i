/***************************************************************************
Version control for database, common definitions, and include files

(c) 1994 - 2000 Innobase Oy

Created 1/20/1994 Heikki Tuuri
****************************************************************************/

#ifndef univ_i
#define univ_i

#define	UNIV_INTEL
#define	UNIV_PENTIUM
/* If UNIV_WINNT is not defined, we assume Windows 95 */

#define	UNIV_WINNT
#define UNIV_WINNT4
#define __NT__

#define UNIV_VISUALC

#define __WIN__
#define _WIN32_WINNT	0x0400

/*			DEBUG VERSION CONTROL
			===================== */
/* Make a non-inline debug version */
/*
#define UNIV_DEBUG
#define UNIV_MEM_DEBUG
#define UNIV_SYNC_DEBUG
#define UNIV_SEARCH_DEBUG

#define UNIV_IBUF_DEBUG

#define UNIV_SEARCH_PERF_STAT
#define UNIV_SYNC_PERF_STAT
*/
#define UNIV_LIGHT_MEM_DEBUG

#define YYDEBUG			1
/*
#define UNIV_SQL_DEBUG
#define UNIV_LOG_DEBUG
*/
			/* the above option prevents forcing of log to disk
			at a buffer page write: it should be tested with this
			option off; also some ibuf tests are suppressed */
/*
#define UNIV_BASIC_LOG_DEBUG
*/
			/* the above option enables basic recovery debugging:
			new allocated file pages are reset */

/* The debug version is slower, thus we may change the length of test loops
depending on the UNIV_DBC parameter */
#ifdef UNIV_DEBUG
#define UNIV_DBC	1
#else
#define	UNIV_DBC	100
#endif

#ifndef UNIV_DEBUG
/* Definition for inline version */

#ifdef UNIV_VISUALC
#define UNIV_INLINE  	__inline
#elif defined(UNIV_GNUC)
#define UNIV_INLINE     extern __inline__
#endif

#else
/* If we want to compile a noninlined version we use the following macro
definitions: */

#define UNIV_NONINL
#define UNIV_INLINE

#endif	/* UNIV_DEBUG */
/* If the compiler does not know inline specifier, we use: */
/*
#define UNIV_INLINE     static
*/


/*
			MACHINE VERSION CONTROL
			=======================
*/

#ifdef UNIV_PENTIUM

/* In a 32-bit computer word size is 4 */
#define UNIV_WORD_SIZE		4

/* The following alignment is used in memory allocations in memory heap
management to ensure correct alignment for doubles etc. */
#define UNIV_MEM_ALIGNMENT      8

/* The following alignment is used in aligning lints etc. */
#define UNIV_WORD_ALIGNMENT	UNIV_WORD_SIZE

#endif

/*
			DATABASE VERSION CONTROL
			========================
*/

/* The universal page size of the database */
#define UNIV_PAGE_SIZE          (2 * 8192)/* NOTE! Currently, this has to be a
					power of 2 and divisible by
					UNIV_MEM_ALIGNMENT */

/* Do non-buffered io in buffer pool read/write operations */
#define UNIV_NON_BUFFERED_IO

/* Maximum number of parallel threads in a parallelized operation */
#define UNIV_MAX_PARALLELISM	32

/*
			UNIVERSAL TYPE DEFINITIONS
			==========================
*/


typedef unsigned char   byte;

/* An other basic type we use is unsigned long integer which is intended to be
equal to the word size of the machine. */

typedef unsigned long int	ulint;

typedef long int		lint;

/* The following type should be at least a 64-bit floating point number */
typedef double		utfloat;

/* The 'undefined' value for a ulint */
#define ULINT_UNDEFINED		((ulint)(-1))

/* The undefined 32-bit unsigned integer */
#define	ULINT32_UNDEFINED	0xFFFFFFFF

/* Maximum value for a ulint */
#define ULINT_MAX		((ulint)(-2))


/* Definition of the boolean type */
typedef ulint    bool;

#define TRUE    1
#define FALSE   0

/* The following number as the length of a logical field means that the field
has the SQL NULL as its value. */
#define UNIV_SQL_NULL 	ULINT_UNDEFINED

#include <stdio.h>
#include "ut0dbg.h"
#include "ut0ut.h"
#include "db0err.h"

#endif
