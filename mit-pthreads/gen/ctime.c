/*
 * Copyright (c) 1987, 1989 Regents of the University of California.
 * Copyright (c) 1994 Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Arthur David Olson of the National Cancer Institute.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)ctime.c	5.26 (Berkeley) 2/23/91";
#endif /* LIBC_SCCS and not lint */

/*
** Leap second handling from Bradley White (bww@k.gp.cs.cmu.edu).
** POSIX-style TZ environment variable handling from Guy Harris
** (guy@auspex.com).
*/

/*LINTLIBRARY*/
#include "config.h"
#include <pthread.h>
#include <sys/param.h>
#include <fcntl.h>
#include <time.h>
#include <tzfile.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#ifndef WILDABBR
/*
** Someone might make incorrect use of a time zone abbreviation:
**	1.	They might reference tzname[0] before calling tzset (explicitly
**	 	or implicitly).
**	2.	They might reference tzname[1] before calling tzset (explicitly
**	 	or implicitly).
**	3.	They might reference tzname[1] after setting to a time zone
**		in which Daylight Saving Time is never observed.
**	4.	They might reference tzname[0] after setting to a time zone
**		in which Standard Time is never observed.
**	5.	They might reference tm.TM_ZONE after calling offtime.
** What's best to do in the above cases is open to debate;
** for now, we just set things up so that in any of the five cases
** WILDABBR is used.  Another possibility:  initialize tzname[0] to the
** string "tzname[0] used before set", and similarly for the other cases.
** And another:  initialize tzname[0] to "ERA", with an explanation in the
** manual page of what this "time zone abbreviation" means (doing this so
** that tzname[0] has the "normal" length of three characters).
*/
#define WILDABBR	"   "
#endif /* !defined WILDABBR */

#ifndef TRUE
#define TRUE		1
#define FALSE		0
#endif /* !defined TRUE */

static const char GMT[] = "GMT";

struct ttinfo {				/* time type information */
	long		tt_gmtoff;	/* GMT offset in seconds */
	int		tt_isdst;	/* used to set tm_isdst */
	int		tt_abbrind;	/* abbreviation list index */
	int		tt_ttisstd;	/* TRUE if transition is std time */
};

struct lsinfo {				/* leap second information */
	time_t		ls_trans;	/* transition time */
	long		ls_corr;	/* correction to apply */
};

struct state {
	int		leapcnt;
	int		timecnt;
	int		typecnt;
	int		charcnt;
	time_t		ats[TZ_MAX_TIMES];
	unsigned char	types[TZ_MAX_TIMES];
	struct ttinfo	ttis[TZ_MAX_TYPES];
	char		chars[(TZ_MAX_CHARS + 1 > sizeof GMT) ?
				TZ_MAX_CHARS + 1 : sizeof GMT];
	struct lsinfo	lsis[TZ_MAX_LEAPS];
};

struct rule {
	int		r_type;		/* type of rule--see below */
	int		r_day;		/* day number of rule */
	int		r_week;		/* week number of rule */
	int		r_mon;		/* month number of rule */
	long		r_time;		/* transition time of rule */
};

#define	JULIAN_DAY		0	/* Jn - Julian day */
#define	DAY_OF_YEAR		1	/* n - day of year */
#define	MONTH_NTH_DAY_OF_WEEK	2	/* Mm.n.d - month, week, day of week */

/*
** Prototypes for static functions.
*/

static long			detzcode __P_((const char *));
static const char *	getnum __P_((const char *, int *, int, int));
static const char *	getsecs __P_((const char *, long *));
static const char *	getoffset __P_((const char *, long *));
static const char *	getrule __P_((const char *, struct rule *));
static const char *	getzname __P_((const char *));
static void			gmtload __P_((struct state *));
static void			gmtsub __P_((const time_t *, long, struct tm *));
static void			localsub __P_((const time_t *, long, struct tm *));
static void			normalize __P_((int *, int *, int));
static void			settzname __P_((struct state *));
static time_t		time1 __P_((struct tm *, long));
static time_t		time2 __P_((struct tm *, long, int *));
static void			timesub __P_((const time_t *, long, const struct state *,
					struct tm *));
static int			tmcomp __P_((const struct tm *, const struct tm *));
static time_t		transtime __P_((time_t, int, const struct rule *, long));
static int			tzload __P_((const char *, struct state *));
static int			tzparse __P_((const char *, struct state *, int));
static void			tzset_basic __P_((void));
static void			tzsetwall_basic __P_((void));

static pthread_mutex_t	lcl_mutex 	= PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t	gmt_mutex 	= PTHREAD_MUTEX_INITIALIZER;
static int 				lcl_is_set 	= FALSE;
static int 				gmt_is_set 	= FALSE;
static struct state		lclmem;
static struct state		gmtmem;

#define	lclptr (&lclmem)
#define	gmtptr (&gmtmem)

char * tzname[2] = {
	WILDABBR,
	WILDABBR
};

#ifdef USG_COMPAT
time_t			timezone = 0;
int			daylight = 0;
#endif /* defined USG_COMPAT */

#ifdef ALTZONE
time_t			altzone = 0;
#endif /* defined ALTZONE */

static long detzcode(const char * codep)
{
	long result;
	int	i;

	result = 0;
	for (i = 0; i < 4; ++i)
		result = (result << 8) | (codep[i] & 0xff);
	return result;
}

static void settzname(struct state * sp)
{
	register int				i;

	tzname[0] = WILDABBR;
	tzname[1] = WILDABBR;
#ifdef USG_COMPAT
	daylight = 0;
	timezone = 0;
#endif /* defined USG_COMPAT */
#ifdef ALTZONE
	altzone = 0;
#endif /* defined ALTZONE */
	for (i = 0; i < sp->typecnt; ++i) {
		register const struct ttinfo * const	ttisp = &sp->ttis[i];

		tzname[ttisp->tt_isdst] =
			(char *) &sp->chars[ttisp->tt_abbrind];
#ifdef USG_COMPAT
		if (ttisp->tt_isdst)
			daylight = 1;
		if (i == 0 || !ttisp->tt_isdst)
			timezone = -(ttisp->tt_gmtoff);
#endif /* defined USG_COMPAT */
#ifdef ALTZONE
		if (i == 0 || ttisp->tt_isdst)
			altzone = -(ttisp->tt_gmtoff);
#endif /* defined ALTZONE */
	}
	/*
	** And to get the latest zone names into tzname. . .
	*/
	for (i = 0; i < sp->timecnt; ++i) {
		register const struct ttinfo * const	ttisp =
							&sp->ttis[sp->types[i]];

		tzname[ttisp->tt_isdst] =
			(char *) &sp->chars[ttisp->tt_abbrind];
	}
}

static int tzload(const char * name, struct state * sp)
{
	register const char *	p;
	register int		i;
	register int		fid;

	if (name == NULL && (name = TZDEFAULT) == NULL)
		return -1;
	{
		char		fullname[FILENAME_MAX + 1];

		if (name[0] == ':')
			++name;
		if (name[0] != '/') {
			if ((p = TZDIR) == NULL)
				return -1;
			if ((strlen(p) + strlen(name) + 1) >= sizeof fullname)
				return -1;
			(void) strcpy(fullname, p);
			(void) strcat(fullname, "/");
			(void) strcat(fullname, name);
			name = fullname;
		}
		if ((fid = open(name, O_RDONLY)) == -1)
			return -1;
	}
	{
		register const struct tzhead *	tzhp;
		char				buf[sizeof *sp + sizeof *tzhp];
		int				ttisstdcnt;

		i = read(fid, buf, sizeof buf);
		if (close(fid) != 0 || i < sizeof *tzhp)
			return -1;
		tzhp = (struct tzhead *) buf;
		ttisstdcnt = (int) detzcode(tzhp->tzh_ttisstdcnt);
		sp->leapcnt = (int) detzcode(tzhp->tzh_leapcnt);
		sp->timecnt = (int) detzcode(tzhp->tzh_timecnt);
		sp->typecnt = (int) detzcode(tzhp->tzh_typecnt);
		sp->charcnt = (int) detzcode(tzhp->tzh_charcnt);
		if (sp->leapcnt < 0 || sp->leapcnt > TZ_MAX_LEAPS ||
			sp->typecnt <= 0 || sp->typecnt > TZ_MAX_TYPES ||
			sp->timecnt < 0 || sp->timecnt > TZ_MAX_TIMES ||
			sp->charcnt < 0 || sp->charcnt > TZ_MAX_CHARS ||
			(ttisstdcnt != sp->typecnt && ttisstdcnt != 0))
				return -1;
		if (i < sizeof *tzhp +
			sp->timecnt * (4 + sizeof (char)) +
			sp->typecnt * (4 + 2 * sizeof (char)) +
			sp->charcnt * sizeof (char) +
			sp->leapcnt * 2 * 4 +
			ttisstdcnt * sizeof (char))
				return -1;
		p = buf + sizeof *tzhp;
		for (i = 0; i < sp->timecnt; ++i) {
			sp->ats[i] = detzcode(p);
			p += 4;
		}
		for (i = 0; i < sp->timecnt; ++i) {
			sp->types[i] = (unsigned char) *p++;
			if (sp->types[i] >= sp->typecnt)
				return -1;
		}
		for (i = 0; i < sp->typecnt; ++i) {
			register struct ttinfo *	ttisp;

			ttisp = &sp->ttis[i];
			ttisp->tt_gmtoff = detzcode(p);
			p += 4;
			ttisp->tt_isdst = (unsigned char) *p++;
			if (ttisp->tt_isdst != 0 && ttisp->tt_isdst != 1)
				return -1;
			ttisp->tt_abbrind = (unsigned char) *p++;
			if (ttisp->tt_abbrind < 0 ||
				ttisp->tt_abbrind > sp->charcnt)
					return -1;
		}
		for (i = 0; i < sp->charcnt; ++i)
			sp->chars[i] = *p++;
		sp->chars[i] = '\0';	/* ensure '\0' at end */
		for (i = 0; i < sp->leapcnt; ++i) {
			register struct lsinfo *	lsisp;

			lsisp = &sp->lsis[i];
			lsisp->ls_trans = detzcode(p);
			p += 4;
			lsisp->ls_corr = detzcode(p);
			p += 4;
		}
		for (i = 0; i < sp->typecnt; ++i) {
			register struct ttinfo *	ttisp;

			ttisp = &sp->ttis[i];
			if (ttisstdcnt == 0)
				ttisp->tt_ttisstd = FALSE;
			else {
				ttisp->tt_ttisstd = *p++;
				if (ttisp->tt_ttisstd != TRUE &&
					ttisp->tt_ttisstd != FALSE)
						return -1;
			}
		}
	}
	return 0;
}

static const int	mon_lengths[2][MONSPERYEAR] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
	31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static const int	year_lengths[2] = {
	DAYSPERNYEAR, DAYSPERLYEAR
};

/*
** Given a pointer into a time zone string, scan until a character that is not
** a valid character in a zone name is found.  Return a pointer to that
** character.
*/
static const char * getzname(const char * strp)
{
	register char	c;

	while ((c = *strp) != '\0' && !isdigit(c) && c != ',' && c != '-' &&
		c != '+')
			++strp;
	return strp;
}

/*
** Given a pointer into a time zone string, extract a number from that string.
** Check that the number is within a specified range; if it is not, return
** NULL.
** Otherwise, return a pointer to the first character not part of the number.
*/

static const char *getnum(const char * strp, int * nump, int min, int max)
{
	char c;
	int	num;

	if (strp == NULL || !isdigit(*strp))
		return NULL;
	num = 0;
	while ((c = *strp) != '\0' && isdigit(c)) {
		num = num * 10 + (c - '0');
		if (num > max)
			return NULL;	/* illegal value */
		++strp;
	}
	if (num < min)
		return NULL;		/* illegal value */
	*nump = num;
	return strp;
}

/*
** Given a pointer into a time zone string, extract a number of seconds,
** in hh[:mm[:ss]] form, from the string.
** If any error occurs, return NULL.
** Otherwise, return a pointer to the first character not part of the number
** of seconds.
*/
static const char * getsecs(const char * strp, long * secsp)
{
	int	num;

	strp = getnum(strp, &num, 0, HOURSPERDAY);
	if (strp == NULL)
		return NULL;
	*secsp = num * SECSPERHOUR;
	if (*strp == ':') {
		++strp;
		strp = getnum(strp, &num, 0, MINSPERHOUR - 1);
		if (strp == NULL)
			return NULL;
		*secsp += num * SECSPERMIN;
		if (*strp == ':') {
			++strp;
			strp = getnum(strp, &num, 0, SECSPERMIN - 1);
			if (strp == NULL)
				return NULL;
			*secsp += num;
		}
	}
	return strp;
}

/*
** Given a pointer into a time zone string, extract an offset, in
** [+-]hh[:mm[:ss]] form, from the string.
** If any error occurs, return NULL.
** Otherwise, return a pointer to the first character not part of the time.
*/
static const char * getoffset(const char * strp, long * offsetp)
{
	int	neg;

	if (*strp == '-') {
		neg = 1;
		++strp;
	} else if (isdigit(*strp) || *strp++ == '+')
		neg = 0;
	else	return NULL;		/* illegal offset */
	strp = getsecs(strp, offsetp);
	if (strp == NULL)
		return NULL;		/* illegal time */
	if (neg)
		*offsetp = -*offsetp;
	return strp;
}

/*
** Given a pointer into a time zone string, extract a rule in the form
** date[/time].  See POSIX section 8 for the format of "date" and "time".
** If a valid rule is not found, return NULL.
** Otherwise, return a pointer to the first character not part of the rule.
*/
static const char * getrule(const char * strp, struct rule * rulep)
{
	if (*strp == 'J') {
		/*
		** Julian day.
		*/
		rulep->r_type = JULIAN_DAY;
		++strp;
		strp = getnum(strp, &rulep->r_day, 1, DAYSPERNYEAR);
	} else if (*strp == 'M') {
		/*
		** Month, week, day.
		*/
		rulep->r_type = MONTH_NTH_DAY_OF_WEEK;
		++strp;
		strp = getnum(strp, &rulep->r_mon, 1, MONSPERYEAR);
		if (strp == NULL)
			return NULL;
		if (*strp++ != '.')
			return NULL;
		strp = getnum(strp, &rulep->r_week, 1, 5);
		if (strp == NULL)
			return NULL;
		if (*strp++ != '.')
			return NULL;
		strp = getnum(strp, &rulep->r_day, 0, DAYSPERWEEK - 1);
	} else if (isdigit(*strp)) {
		/*
		** Day of year.
		*/
		rulep->r_type = DAY_OF_YEAR;
		strp = getnum(strp, &rulep->r_day, 0, DAYSPERLYEAR - 1);
	} else	return NULL;		/* invalid format */
	if (strp == NULL)
		return NULL;
	if (*strp == '/') {
		/*
		** Time specified.
		*/
		++strp;
		strp = getsecs(strp, &rulep->r_time);
	} else	rulep->r_time = 2 * SECSPERHOUR;	/* default = 2:00:00 */
	return strp;
}

/*
** Given the Epoch-relative time of January 1, 00:00:00 GMT, in a year, the
** year, a rule, and the offset from GMT at the time that rule takes effect,
** calculate the Epoch-relative time that rule takes effect.
*/
static time_t transtime(time_t janfirst, int year,
  const struct rule * rulep, long offset)
{
	register int	leapyear;
	register time_t	value;
	register int	i;
	int		d, m1, yy0, yy1, yy2, dow;

	leapyear = isleap(year);
	switch (rulep->r_type) {

	case JULIAN_DAY:
		/*
		** Jn - Julian day, 1 == January 1, 60 == March 1 even in leap
		** years.
		** In non-leap years, or if the day number is 59 or less, just
		** add SECSPERDAY times the day number-1 to the time of
		** January 1, midnight, to get the day.
		*/
		value = janfirst + (rulep->r_day - 1) * SECSPERDAY;
		if (leapyear && rulep->r_day >= 60)
			value += SECSPERDAY;
		break;

	case DAY_OF_YEAR:
		/*
		** n - day of year.
		** Just add SECSPERDAY times the day number to the time of
		** January 1, midnight, to get the day.
		*/
		value = janfirst + rulep->r_day * SECSPERDAY;
		break;

	case MONTH_NTH_DAY_OF_WEEK:
		/*
		** Mm.n.d - nth "dth day" of month m.
		*/
		value = janfirst;
		for (i = 0; i < rulep->r_mon - 1; ++i)
			value += mon_lengths[leapyear][i] * SECSPERDAY;

		/*
		** Use Zeller's Congruence to get day-of-week of first day of
		** month.
		*/
		m1 = (rulep->r_mon + 9) % 12 + 1;
		yy0 = (rulep->r_mon <= 2) ? (year - 1) : year;
		yy1 = yy0 / 100;
		yy2 = yy0 % 100;
		dow = ((26 * m1 - 2) / 10 +
			1 + yy2 + yy2 / 4 + yy1 / 4 - 2 * yy1) % 7;
		if (dow < 0)
			dow += DAYSPERWEEK;

		/*
		** "dow" is the day-of-week of the first day of the month.  Get
		** the day-of-month (zero-origin) of the first "dow" day of the
		** month.
		*/
		d = rulep->r_day - dow;
		if (d < 0)
			d += DAYSPERWEEK;
		for (i = 1; i < rulep->r_week; ++i) {
			if (d + DAYSPERWEEK >=
				mon_lengths[leapyear][rulep->r_mon - 1])
					break;
			d += DAYSPERWEEK;
		}

		/*
		** "d" is the day-of-month (zero-origin) of the day we want.
		*/
		value += d * SECSPERDAY;
		break;
	}

	/*
	** "value" is the Epoch-relative time of 00:00:00 GMT on the day in
	** question.  To get the Epoch-relative time of the specified local
	** time on that day, add the transition time and the current offset
	** from GMT.
	*/
	return value + rulep->r_time + offset;
}

/*
** Given a POSIX section 8-style TZ string, fill in the rule tables as
** appropriate.
*/
static int tzparse(const char * name, struct state * sp, int lastditch)
{
	const char *			stdname;
	const char *			dstname;
	int				stdlen;
	int				dstlen;
	long				stdoffset;
	long				dstoffset;
	register time_t *		atp;
	register unsigned char *	typep;
	register char *			cp;
	register int			load_result;

	stdname = name;
	if (lastditch) {
		stdlen = strlen(name);	/* length of standard zone name */
		name += stdlen;
		if (stdlen >= sizeof sp->chars)
			stdlen = (sizeof sp->chars) - 1;
	} else {
		name = getzname(name);
		stdlen = name - stdname;
		if (stdlen < 3)
			return -1;
	}
	if (*name == '\0')
		return -1;
	else {
		name = getoffset(name, &stdoffset);
		if (name == NULL)
			return -1;
	}
	load_result = tzload(TZDEFRULES, sp);
	if (load_result != 0)
		sp->leapcnt = 0;		/* so, we're off a little */
	if (*name != '\0') {
		dstname = name;
		name = getzname(name);
		dstlen = name - dstname;	/* length of DST zone name */
		if (dstlen < 3)
			return -1;
		if (*name != '\0' && *name != ',' && *name != ';') {
			name = getoffset(name, &dstoffset);
			if (name == NULL)
				return -1;
		} else	dstoffset = stdoffset - SECSPERHOUR;
		if (*name == ',' || *name == ';') {
			struct rule	start;
			struct rule	end;
			register int	year;
			register time_t	janfirst;
			time_t		starttime;
			time_t		endtime;

			++name;
			if ((name = getrule(name, &start)) == NULL)
				return -1;
			if (*name++ != ',')
				return -1;
			if ((name = getrule(name, &end)) == NULL)
				return -1;
			if (*name != '\0')
				return -1;
			sp->typecnt = 2;	/* standard time and DST */
			/*
			** Two transitions per year, from EPOCH_YEAR to 2037.
			*/
			sp->timecnt = 2 * (2037 - EPOCH_YEAR + 1);
			if (sp->timecnt > TZ_MAX_TIMES)
				return -1;
			sp->ttis[0].tt_gmtoff = -dstoffset;
			sp->ttis[0].tt_isdst = 1;
			sp->ttis[0].tt_abbrind = stdlen + 1;
			sp->ttis[1].tt_gmtoff = -stdoffset;
			sp->ttis[1].tt_isdst = 0;
			sp->ttis[1].tt_abbrind = 0;
			atp = sp->ats;
			typep = sp->types;
			janfirst = 0;
			for (year = EPOCH_YEAR; year <= 2037; ++year) {
				starttime = transtime(janfirst, year, &start,
					stdoffset);
				endtime = transtime(janfirst, year, &end,
					dstoffset);
				if (starttime > endtime) {
					*atp++ = endtime;
					*typep++ = 1;	/* DST ends */
					*atp++ = starttime;
					*typep++ = 0;	/* DST begins */
				} else {
					*atp++ = starttime;
					*typep++ = 0;	/* DST begins */
					*atp++ = endtime;
					*typep++ = 1;	/* DST ends */
				}
				janfirst +=
					year_lengths[isleap(year)] * SECSPERDAY;
			}
		} else {
			int		sawstd;
			int		sawdst;
			long		stdfix;
			long		dstfix;
			long		oldfix;
			int		isdst;
			register int	i;

			if (*name != '\0')
				return -1;
			if (load_result != 0)
				return -1;
			/*
			** Compute the difference between the real and
			** prototype standard and summer time offsets
			** from GMT, and put the real standard and summer
			** time offsets into the rules in place of the
			** prototype offsets.
			*/
			sawstd = FALSE;
			sawdst = FALSE;
			stdfix = 0;
			dstfix = 0;
			for (i = 0; i < sp->typecnt; ++i) {
				if (sp->ttis[i].tt_isdst) {
					oldfix = dstfix;
					dstfix =
					    sp->ttis[i].tt_gmtoff + dstoffset;
					if (sawdst && (oldfix != dstfix))
						return -1;
					sp->ttis[i].tt_gmtoff = -dstoffset;
					sp->ttis[i].tt_abbrind = stdlen + 1;
					sawdst = TRUE;
				} else {
					oldfix = stdfix;
					stdfix =
					    sp->ttis[i].tt_gmtoff + stdoffset;
					if (sawstd && (oldfix != stdfix))
						return -1;
					sp->ttis[i].tt_gmtoff = -stdoffset;
					sp->ttis[i].tt_abbrind = 0;
					sawstd = TRUE;
				}
			}
			/*
			** Make sure we have both standard and summer time.
			*/
			if (!sawdst || !sawstd)
				return -1;
			/*
			** Now correct the transition times by shifting
			** them by the difference between the real and
			** prototype offsets.  Note that this difference
			** can be different in standard and summer time;
			** the prototype probably has a 1-hour difference
			** between standard and summer time, but a different
			** difference can be specified in TZ.
			*/
			isdst = FALSE;	/* we start in standard time */
			for (i = 0; i < sp->timecnt; ++i) {
				register const struct ttinfo *	ttisp;

				/*
				** If summer time is in effect, and the
				** transition time was not specified as
				** standard time, add the summer time
				** offset to the transition time;
				** otherwise, add the standard time offset
				** to the transition time.
				*/
				ttisp = &sp->ttis[sp->types[i]];
				sp->ats[i] +=
					(isdst && !ttisp->tt_ttisstd) ?
						dstfix : stdfix;
				isdst = ttisp->tt_isdst;
			}
		}
	} else {
		dstlen = 0;
		sp->typecnt = 1;		/* only standard time */
		sp->timecnt = 0;
		sp->ttis[0].tt_gmtoff = -stdoffset;
		sp->ttis[0].tt_isdst = 0;
		sp->ttis[0].tt_abbrind = 0;
	}
	sp->charcnt = stdlen + 1;
	if (dstlen != 0)
		sp->charcnt += dstlen + 1;
	if (sp->charcnt > sizeof sp->chars)
		return -1;
	cp = sp->chars;
	(void) strncpy(cp, stdname, stdlen);
	cp += stdlen;
	*cp++ = '\0';
	if (dstlen != 0) {
		(void) strncpy(cp, dstname, dstlen);
		*(cp + dstlen) = '\0';
	}
	return 0;
}

static void gmtload(struct state * sp)
{
	if (tzload(GMT, sp) != 0)
		(void) tzparse(GMT, sp, TRUE);
}

static void tzset_basic()
{
	const char * name;
	if ((name = getenv("TZ")) == NULL) {
		tzsetwall_basic();
		return;
	}

	if (*name == '\0') {
		/*
		** User wants it fast rather than right.
		*/
		lclptr->leapcnt = 0;		/* so, we're off a little */
		lclptr->timecnt = 0;
		lclptr->ttis[0].tt_gmtoff = 0;
		lclptr->ttis[0].tt_abbrind = 0;
		(void) strcpy(lclptr->chars, GMT);
	} else {
		if (tzload(name, lclptr) != 0)
			if (name[0] == ':' || tzparse(name, lclptr, FALSE) != 0)
				(void) gmtload(lclptr);
	}
	lcl_is_set = TRUE;
	settzname(lclptr);
}

void tzset()
{
	pthread_mutex_lock(&lcl_mutex);
	tzset_basic();
	pthread_mutex_unlock(&lcl_mutex);
}

static void tzsetwall_basic()
{
	if (tzload((char *) NULL, lclptr) != 0)
		gmtload(lclptr);
	settzname(lclptr);
	lcl_is_set = TRUE;
}

void tzsetwall()
{
	pthread_mutex_lock(&lcl_mutex);
	tzsetwall_basic();
	pthread_mutex_unlock(&lcl_mutex);
}

/*
** The easy way to behave "as if no library function calls" localtime
** is to not call it--so we drop its guts into "localsub", which can be
** freely called.  (And no, the PANS doesn't require the above behavior--
** but it *is* desirable.)
**
** The unused offset argument is for the benefit of mktime variants.
*/

static void localsub(const time_t * timep, long offset, struct tm * tmp)
{
	const struct ttinfo * ttisp;
	const time_t t = *timep;
	struct state * sp;
	int	i;

	if (!lcl_is_set)
		tzset_basic();
	sp = lclptr;
	if (sp->timecnt == 0 || t < sp->ats[0]) {
		i = 0;
		while (sp->ttis[i].tt_isdst)
			if (++i >= sp->typecnt) {
				i = 0;
				break;
			}
	} else {
		for (i = 1; i < sp->timecnt; ++i)
			if (t < sp->ats[i])
				break;
		i = sp->types[i - 1];
	}
	ttisp = &sp->ttis[i];
	/*
	** To get (wrong) behavior that's compatible with System V Release 2.0
	** you'd replace the statement below with
	**	t += ttisp->tt_gmtoff;
	**	timesub(&t, 0L, sp, tmp);
	*/
	timesub(&t, ttisp->tt_gmtoff, sp, tmp);
	tzname[tmp->tm_isdst] = (char *) &sp->chars[ttisp->tt_abbrind];
#ifdef BSD_TM
	tmp->tm_zone = &sp->chars[ttisp->tt_abbrind];
#endif
	tmp->tm_isdst = ttisp->tt_isdst;

	pthread_mutex_unlock(&lcl_mutex);
}

struct tm * localtime_r(const time_t * timep, struct tm * tm)
{
	pthread_mutex_lock(&lcl_mutex);
	localsub(timep, 0L, tm);
	pthread_mutex_unlock(&lcl_mutex);
	return(tm);
}

struct tm * localtime(const time_t * timep)
{
	static pthread_mutex_t localtime_mutex = PTHREAD_MUTEX_INITIALIZER;
	static pthread_key_t localtime_key = -1;
	struct tm * tm;

	pthread_mutex_lock(&localtime_mutex);
	if (localtime_key < 0) {
		if (pthread_key_create(&localtime_key, free) < 0) {
			pthread_mutex_unlock(&localtime_mutex);
			return(NULL);
		}
	}
	pthread_mutex_unlock(&localtime_mutex);
	if ((tm = pthread_getspecific(localtime_key)) == NULL) {
		if ((tm = (struct tm *)malloc(sizeof(struct tm))) == NULL) {
			return(NULL);
		}
		pthread_setspecific(localtime_key, tm);
	}

	pthread_mutex_lock(&lcl_mutex);
	localsub(timep, 0L, tm);
	pthread_mutex_unlock(&lcl_mutex);
	return tm;
}


/*
 * gmtsub is to gmtime as localsub is to localtime.
 *
 * Once set there is no need to lock the gmt_mutex to view gmtptr 
 */
static void gmtsub(const time_t * timep, long offset, struct tm * tmp)
{
	pthread_mutex_lock(&gmt_mutex);
	if (gmt_is_set == FALSE) {
		gmt_is_set = TRUE;
		gmtload(gmtptr);
	}
	pthread_mutex_unlock(&gmt_mutex);
	
	timesub(timep, offset, gmtptr, tmp);
	/*
	** Could get fancy here and deliver something such as
	** "GMT+xxxx" or "GMT-xxxx" if offset is non-zero,
	** but this is no time for a treasure hunt.
	*/
#ifdef BSD_TM
	if (offset != 0) {
		tmp->tm_zone = WILDABBR;
	} else {
		tmp->tm_zone = gmtptr->chars;
	}
#endif
}

struct tm * gmtime_r(const time_t * timep, struct tm * tm)
{
	gmtsub(timep, 0L, tm);
	return(tm);
}

struct tm * gmtime(const time_t * timep)
{
	static pthread_mutex_t gmtime_mutex = PTHREAD_MUTEX_INITIALIZER;
	static pthread_key_t gmtime_key = -1;
	struct tm * tm;

	pthread_mutex_lock(&gmtime_mutex);
	if (gmtime_key < 0) {
		if (pthread_key_create(&gmtime_key, free) < 0) {
			pthread_mutex_unlock(&gmtime_mutex);
			return(NULL);
		}
	}
	pthread_mutex_unlock(&gmtime_mutex);
	if ((tm = pthread_getspecific(gmtime_key)) == NULL) {
		if ((tm = (struct tm *)malloc(sizeof(struct tm))) == NULL) {
			return(NULL);
		}
		pthread_setspecific(gmtime_key, tm);
	}

	gmtsub(timep, 0L, tm);
	return(tm);
}

static void timesub(const time_t * timep, long offset,
  const struct state * sp, struct tm * tmp)
{
	register const struct lsinfo *	lp;
	register long			days;
	register long			rem;
	register int			y;
	register int			yleap;
	register const int *		ip;
	register long			corr;
	register int			hit;
	register int			i;

	corr = 0;
	hit = FALSE;
	i = sp->leapcnt;
	while (--i >= 0) {
		lp = &sp->lsis[i];
		if (*timep >= lp->ls_trans) {
			if (*timep == lp->ls_trans)
				hit = ((i == 0 && lp->ls_corr > 0) ||
					lp->ls_corr > sp->lsis[i - 1].ls_corr);
			corr = lp->ls_corr;
			break;
		}
	}
	days = *timep / SECSPERDAY;
	rem = *timep % SECSPERDAY;
#ifdef mc68k
	if (*timep == 0x80000000) {
		/*
		** A 3B1 muffs the division on the most negative number.
		*/
		days = -24855;
		rem = -11648;
	}
#endif /* mc68k */
	rem += (offset - corr);
	while (rem < 0) {
		rem += SECSPERDAY;
		--days;
	}
	while (rem >= SECSPERDAY) {
		rem -= SECSPERDAY;
		++days;
	}
	tmp->tm_hour = (int) (rem / SECSPERHOUR);
	rem = rem % SECSPERHOUR;
	tmp->tm_min = (int) (rem / SECSPERMIN);
	tmp->tm_sec = (int) (rem % SECSPERMIN);
	if (hit)
		/*
		** A positive leap second requires a special
		** representation.  This uses "... ??:59:60".
		*/
		++(tmp->tm_sec);
	tmp->tm_wday = (int) ((EPOCH_WDAY + days) % DAYSPERWEEK);
	if (tmp->tm_wday < 0)
		tmp->tm_wday += DAYSPERWEEK;
	y = EPOCH_YEAR;
	if (days >= 0)
		for ( ; ; ) {
			yleap = isleap(y);
			if (days < (long) year_lengths[yleap])
				break;
			++y;
			days = days - (long) year_lengths[yleap];
		}
	else do {
		--y;
		yleap = isleap(y);
		days = days + (long) year_lengths[yleap];
	} while (days < 0);
	tmp->tm_year = y - TM_YEAR_BASE;
	tmp->tm_yday = (int) days;
	ip = mon_lengths[yleap];
	for (tmp->tm_mon = 0; days >= (long) ip[tmp->tm_mon]; ++(tmp->tm_mon))
		days = days - (long) ip[tmp->tm_mon];
	tmp->tm_mday = (int) (days + 1);
	tmp->tm_isdst = 0;
#ifdef BSD_TM
	tmp->tm_gmtoff = offset;
#endif
}

/*
 * A la X3J11
 *
 * Made thread safe by using thread specific data
 */
char * asctime_r(const struct tm * timeptr, char * result)
{
	static const char	wday_name[DAYSPERWEEK][3] = {
		"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
	};
	static const char	mon_name[MONSPERYEAR][3] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};
	(void) sprintf(result, "%.3s %.3s%3d %02.2d:%02.2d:%02.2d %d\n",
		wday_name[timeptr->tm_wday],
		mon_name[timeptr->tm_mon],
		timeptr->tm_mday, timeptr->tm_hour,
		timeptr->tm_min, timeptr->tm_sec,
		TM_YEAR_BASE + timeptr->tm_year);
	return(result);
}

char * asctime(const struct tm * timeptr)
{
	static pthread_mutex_t asctime_mutex = PTHREAD_MUTEX_INITIALIZER;
	static pthread_key_t asctime_key = -1;
	char * result;

	pthread_mutex_lock(&asctime_mutex);
	if (asctime_key < 0) {
		if (pthread_key_create(&asctime_key, free) < 0) {
			pthread_mutex_unlock(&asctime_mutex);
			return(NULL);
		}
	}
	pthread_mutex_unlock(&asctime_mutex);
	if ((result = pthread_getspecific(asctime_key)) == NULL) {
		if ((result = malloc(26)) == NULL) {
			return(NULL);
		}
		pthread_setspecific(asctime_key, result);
	}

	return(asctime_r(timeptr, result));
}

char * ctime_r(const time_t * timep, char * buf)
{
	struct tm tm;
	return asctime_r(localtime_r(timep, &tm), buf);
}

char * ctime(const time_t * timep)
{
	struct tm tm;
	return asctime(localtime_r(timep, &tm));
}

/*
** Adapted from code provided by Robert Elz, who writes:
**	The "best" way to do mktime I think is based on an idea of Bob
**	Kridle's (so its said...) from a long time ago. (mtxinu!kridle now).
**	It does a binary search of the time_t space.  Since time_t's are
**	just 32 bits, its a max of 32 iterations (even at 64 bits it
**	would still be very reasonable).
*/
static void normalize(int * tensptr,int *  unitsptr, int base)
{
	if (*unitsptr >= base) {
		*tensptr += *unitsptr / base;
		*unitsptr %= base;
	} else if (*unitsptr < 0) {
		--*tensptr;
		*unitsptr += base;
		if (*unitsptr < 0) {
			*tensptr -= 1 + (-*unitsptr) / base;
			*unitsptr = base - (-*unitsptr) % base;
		}
	}
}

static int tmcomp(const struct tm * atmp, const struct tm * btmp)
{
	register int	result;

	if ((result = (atmp->tm_year - btmp->tm_year)) == 0 &&
		(result = (atmp->tm_mon - btmp->tm_mon)) == 0 &&
		(result = (atmp->tm_mday - btmp->tm_mday)) == 0 &&
		(result = (atmp->tm_hour - btmp->tm_hour)) == 0 &&
		(result = (atmp->tm_min - btmp->tm_min)) == 0)
			result = atmp->tm_sec - btmp->tm_sec;
	return result;
}

static time_t time2(struct tm * tmp, long offset, int * okayp)
{
	register const struct state *	sp;
	register int			dir;
	register int			bits;
	register int			i, j ;
	register int			saved_seconds;
	time_t				newt;
	time_t				t;
	struct tm			yourtm, mytm;

	*okayp = FALSE;
	yourtm = *tmp;
	if (yourtm.tm_sec >= SECSPERMIN + 2 || yourtm.tm_sec < 0)
		normalize(&yourtm.tm_min, &yourtm.tm_sec, SECSPERMIN);
	normalize(&yourtm.tm_hour, &yourtm.tm_min, MINSPERHOUR);
	normalize(&yourtm.tm_mday, &yourtm.tm_hour, HOURSPERDAY);
	normalize(&yourtm.tm_year, &yourtm.tm_mon, MONSPERYEAR);
	while (yourtm.tm_mday <= 0) {
		--yourtm.tm_year;
		yourtm.tm_mday +=
			year_lengths[isleap(yourtm.tm_year + TM_YEAR_BASE)];
	}
	for ( ; ; ) {
		i = mon_lengths[isleap(yourtm.tm_year +
			TM_YEAR_BASE)][yourtm.tm_mon];
		if (yourtm.tm_mday <= i)
			break;
		yourtm.tm_mday -= i;
		if (++yourtm.tm_mon >= MONSPERYEAR) {
			yourtm.tm_mon = 0;
			++yourtm.tm_year;
		}
	}
	saved_seconds = yourtm.tm_sec;
	yourtm.tm_sec = 0;
	/*
	** Calculate the number of magnitude bits in a time_t
	** (this works regardless of whether time_t is
	** signed or unsigned, though lint complains if unsigned).
	*/
	for (bits = 0, t = 1; t > 0; ++bits, t <<= 1)
		;
	/*
	** If time_t is signed, then 0 is the median value,
	** if time_t is unsigned, then 1 << bits is median.
	*/
	t = (t < 0) ? 0 : ((time_t) 1 << bits);
	for ( ; ; ) {
		localsub(&t, offset, &mytm);
		dir = tmcomp(&mytm, &yourtm);
		if (dir != 0) {
			if (bits-- < 0)
				return NOTOK;
			if (bits < 0)
				--t;
			else if (dir > 0)
				t -= (time_t) 1 << bits;
			else	t += (time_t) 1 << bits;
			continue;
		}
		if (yourtm.tm_isdst < 0 || mytm.tm_isdst == yourtm.tm_isdst)
			break;
		/*
		** Right time, wrong type.
		** Hunt for right time, right type.
		** It's okay to guess wrong since the guess
		** gets checked.
		*/
		sp = lclptr;
		for (i = 0; i < sp->typecnt; ++i) {
			if (sp->ttis[i].tt_isdst != yourtm.tm_isdst)
				continue;
			for (j = 0; j < sp->typecnt; ++j) {
				if (sp->ttis[j].tt_isdst == yourtm.tm_isdst)
					continue;
				newt = t + sp->ttis[j].tt_gmtoff -
					sp->ttis[i].tt_gmtoff;
				localsub(&newt, offset, &mytm);
				if (tmcomp(&mytm, &yourtm) != 0)
					continue;
				if (mytm.tm_isdst != yourtm.tm_isdst)
					continue;
				/*
				** We have a match.
				*/
				t = newt;
				goto label;
			}
		}
		return NOTOK;
	}
label:
	t += saved_seconds;
	localsub(&t, offset, tmp);
	*okayp = TRUE;
	return t;
}

static time_t time1(struct tm * tmp, long offset)
{
	const struct state * sp;
	int	samei, otheri, okay;
	time_t t;

	if (tmp->tm_isdst > 1)
		tmp->tm_isdst = 1;
	t = time2(tmp, offset, &okay);
	if (okay || tmp->tm_isdst < 0)
		return t;
	/*
	** We're supposed to assume that somebody took a time of one type
	** and did some math on it that yielded a "struct tm" that's bad.
	** We try to divine the type they started from and adjust to the
	** type they need.
	*/
	sp = lclptr;
	for (samei = 0; samei < sp->typecnt; ++samei) {
		if (sp->ttis[samei].tt_isdst != tmp->tm_isdst)
			continue;
		for (otheri = 0; otheri < sp->typecnt; ++otheri) {
			if (sp->ttis[otheri].tt_isdst == tmp->tm_isdst)
				continue;
			tmp->tm_sec += sp->ttis[otheri].tt_gmtoff -
					sp->ttis[samei].tt_gmtoff;
			tmp->tm_isdst = !tmp->tm_isdst;
			t = time2(tmp, offset, &okay);
			if (okay) 
				return t;
			tmp->tm_sec -= sp->ttis[otheri].tt_gmtoff -
					sp->ttis[samei].tt_gmtoff;
			tmp->tm_isdst = !tmp->tm_isdst;
		}
	}
	return NOTOK;
}

time_t mktime(struct tm * tmp)
{
	time_t mktime_return_value;

	pthread_mutex_lock(&lcl_mutex);
	if (lcl_is_set == FALSE) {
		tzset_basic();
	}
	mktime_return_value = time1(tmp, 0L);
	pthread_mutex_unlock(&lcl_mutex);
	return(mktime_return_value);
}
