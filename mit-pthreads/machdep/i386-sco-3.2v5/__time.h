/* Copyright 1994-1995 The Santa Cruz Operation, Inc. All Rights Reserved. */


#if defined(_NO_PROTOTYPE)	/* Old, crufty environment */
#include <oldstyle/time.h>
#elif defined(_XOPEN_SOURCE) || defined(_XPG4_VERS)	/* Xpg4 environment */
#include <xpg4/time.h>
#elif defined(_POSIX_SOURCE) || defined(_POSIX_C_SOURCE) /* Posix environment */
#include <posix/time.h>
#elif _STRICT_ANSI 	/* Pure Ansi/ISO environment */
#include <ansi/time.h>
#elif defined(_SCO_ODS_30) /* Old, Tbird compatible environment */
#include <ods_30_compat/time.h>
#else 	/* Normal, default environment */
/*
 *   Portions Copyright (C) 1983-1995 The Santa Cruz Operation, Inc.
 *		All Rights Reserved.
 *
 *	The information in this file is provided for the exclusive use of
 *	the licensees of The Santa Cruz Operation, Inc.  Such users have the
 *	right to use, modify, and incorporate this code into other products
 *	for purposes authorized by the license agreement provided they include
 *	this notice and the associated copyright notice with any such product.
 *	The information in this file is provided "AS IS" without warranty.
 */

/*	Portions Copyright (c) 1990, 1991, 1992, 1993 UNIX System Laboratories, Inc. */
/*	Portions Copyright (c) 1979 - 1990 AT&T   */
/*	  All Rights Reserved   */

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF          */
/*	UNIX System Laboratories, Inc.                          */
/*	The copyright notice above does not evidence any        */
/*	actual or intended publication of such source code.     */

#ifndef ___TIME_H
#define ___TIME_H

#pragma comment(exestr, "xpg4plus @(#) time.h 20.2 95/01/04 ")

#ifdef __cplusplus
extern "C" {
#endif

#ifndef NULL
#define NULL	0
#endif /* NULL */

#ifndef _SIZE_T
#define _SIZE_T
typedef unsigned int size_t;
#endif
#ifndef _CLOCK_T
#define _CLOCK_T
typedef long clock_t;
#endif
#ifndef _TIME_T
#define _TIME_T
typedef long time_t;
#endif

#ifdef _POSIXTIMERS
#include <sys/sudstime.h>
#endif

#define CLOCKS_PER_SEC	1000000		/* As required by XPG4 and friends */

#pragma pack(4)

#ifndef _STRUCT_TM
#define _STRUCT_TM
struct tm
{
	int	tm_sec;
	int	tm_min;
	int	tm_hour;
	int	tm_mday;
	int	tm_mon;
	int	tm_year;
	int	tm_wday;
	int	tm_yday;
	int	tm_isdst;
#define LTZNMAX 50
	long tm_tzadj;
	char tm_name[LTZNMAX];      /* name of timezone  */
};

#pragma pack()
#endif /* _STRUCT_TM */


extern clock_t	clock(void);
extern double	difftime(time_t, time_t);
extern time_t	mktime(struct tm *);
extern time_t	time(time_t *);
extern char	*asctime(const struct tm *);
extern char	*ctime (const time_t *);
extern struct tm	*gmtime(const time_t *);
extern struct tm	*localtime(const time_t *);
extern size_t	strftime(char *, size_t, const char *, const struct tm *);


extern void	tzset(void);
extern char	*tzname[];

#ifndef CLK_TCK
#define CLK_TCK _sysconf(2)	/* 2 is _SC_CLK_TCK  */
#endif

extern long	timezone;
extern int	daylight;
extern char	*strptime(const char *, const char *, struct tm *);




#include <sys/timeb.h>
extern int	ftime ( struct timeb * );
extern char *   nl_cxtime( long *, char * );
extern char *   nl_ascxtime( struct tm *, char * );
extern int	cftime(char *, const char *, const time_t *);
extern int	ascftime(char *, const char *, const struct tm *);
extern long	altzone;
extern struct tm	*getdate(const char *);
extern int	getdate_err;
extern char	*asctime_r(const struct tm *, char *,int);
extern char	*ctime_r(const time_t *, char *,int);
extern struct tm	*localtime_r(const time_t *, struct tm *);
extern struct tm	*gmtime_r(const time_t *, struct tm *);


#ifdef __cplusplus
}
#endif

#ifndef difftime
#define difftime(t1, t0) ((double)((t1) - (t0)))
#endif

#endif /* ___TIME_H */
#endif
