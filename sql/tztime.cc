/* Copyright (C) 2004 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */

/*
   Most of the following code and structures were derived from
   public domain code from ftp://elsie.nci.nih.gov/pub
   (We will refer to this code as to elsie-code further.)
*/

/*
  We should not include mysql_priv.h in mysql_tzinfo_to_sql utility since
  it creates unsolved link dependencies on some platforms.
*/

#ifdef USE_PRAGMA_IMPLEMENTATION
#pragma implementation				// gcc: Class implementation
#endif

#include <my_global.h>
#if !defined(TZINFO2SQL) && !defined(TESTTIME)
#include "mysql_priv.h"
#else
#include <my_time.h>
#include "tztime.h"
#include <my_sys.h>
#endif

#include "tzfile.h"
#include <m_string.h>
#include <my_dir.h>

/*
  Now we don't use abbreviations in server but we will do this in future.
*/
#if defined(TZINFO2SQL) || defined(TESTTIME)
#define ABBR_ARE_USED
#else
#if !defined(DBUG_OFF)
/* Let use abbreviations for debug purposes */
#undef ABBR_ARE_USED
#define ABBR_ARE_USED
#endif /* !defined(DBUG_OFF) */
#endif /* defined(TZINFO2SQL) || defined(TESTTIME) */

/* Structure describing local time type (e.g. Moscow summer time (MSD)) */
typedef struct ttinfo
{
  long tt_gmtoff; // Offset from UTC in seconds
  uint tt_isdst;   // Is daylight saving time or not. Used to set tm_isdst
#ifdef ABBR_ARE_USED
  uint tt_abbrind; // Index of start of abbreviation for this time type.
#endif
  /*
    We don't use tt_ttisstd and tt_ttisgmt members of original elsie-code
    struct since we don't support POSIX-style TZ descriptions in variables.
  */
} TRAN_TYPE_INFO;

/* Structure describing leap-second corrections. */
typedef struct lsinfo
{
  my_time_t ls_trans; // Transition time
  long      ls_corr;  // Correction to apply
} LS_INFO;

/*
  Structure with information describing ranges of my_time_t shifted to local
  time (my_time_t + offset). Used for local MYSQL_TIME -> my_time_t conversion.
  See comments for TIME_to_gmt_sec() for more info.
*/
typedef struct revtinfo
{
  long rt_offset; // Offset of local time from UTC in seconds
  uint rt_type;    // Type of period 0 - Normal period. 1 - Spring time-gap
} REVT_INFO;

#ifdef TZNAME_MAX
#define MY_TZNAME_MAX	TZNAME_MAX
#endif
#ifndef TZNAME_MAX
#define MY_TZNAME_MAX	255
#endif

/*
  Structure which fully describes time zone which is
  described in our db or in zoneinfo files.
*/
typedef struct st_time_zone_info
{
  uint leapcnt;  // Number of leap-second corrections
  uint timecnt;  // Number of transitions between time types
  uint typecnt;  // Number of local time types
  uint charcnt;  // Number of characters used for abbreviations
  uint revcnt;   // Number of transition descr. for TIME->my_time_t conversion
  /* The following are dynamical arrays are allocated in MEM_ROOT */
  my_time_t *ats;       // Times of transitions between time types
  uchar	*types; // Local time types for transitions
  TRAN_TYPE_INFO *ttis; // Local time types descriptions
#ifdef ABBR_ARE_USED
  /* Storage for local time types abbreviations. They are stored as ASCIIZ */
  char *chars;
#endif
  /*
    Leap seconds corrections descriptions, this array is shared by
    all time zones who use leap seconds.
  */
  LS_INFO *lsis;
  /*
    Starting points and descriptions of shifted my_time_t (my_time_t + offset)
    ranges on which shifted my_time_t -> my_time_t mapping is linear or undefined.
    Used for tm -> my_time_t conversion.
  */
  my_time_t *revts;
  REVT_INFO *revtis;
  /*
    Time type which is used for times smaller than first transition or if
    there are no transitions at all.
  */
  TRAN_TYPE_INFO *fallback_tti;

} TIME_ZONE_INFO;


static my_bool prepare_tz_info(TIME_ZONE_INFO *sp, MEM_ROOT *storage);


#if defined(TZINFO2SQL) || defined(TESTTIME)

/*
  Load time zone description from zoneinfo (TZinfo) file.

  SYNOPSIS
    tz_load()
      name - path to zoneinfo file
      sp   - TIME_ZONE_INFO structure to fill

  RETURN VALUES
    0 - Ok
    1 - Error
*/
static my_bool
tz_load(const char *name, TIME_ZONE_INFO *sp, MEM_ROOT *storage)
{
  uchar *p;
  int read_from_file;
  uint i;
  FILE *file;

  if (!(file= my_fopen(name, O_RDONLY|O_BINARY, MYF(MY_WME))))
    return 1;
  {
    union
    {
      struct tzhead tzhead;
      uchar buf[sizeof(struct tzhead) + sizeof(my_time_t) * TZ_MAX_TIMES +
                TZ_MAX_TIMES + sizeof(TRAN_TYPE_INFO) * TZ_MAX_TYPES +
#ifdef ABBR_ARE_USED
               max(TZ_MAX_CHARS + 1, (2 * (MY_TZNAME_MAX + 1))) +
#endif
               sizeof(LS_INFO) * TZ_MAX_LEAPS];
    } u;
    uint ttisstdcnt;
    uint ttisgmtcnt;
    char *tzinfo_buf;

    read_from_file= my_fread(file, u.buf, sizeof(u.buf), MYF(MY_WME));

    if (my_fclose(file, MYF(MY_WME)) != 0)
      return 1;

    if (read_from_file < (int)sizeof(struct tzhead))
      return 1;

    ttisstdcnt= int4net(u.tzhead.tzh_ttisgmtcnt);
    ttisgmtcnt= int4net(u.tzhead.tzh_ttisstdcnt);
    sp->leapcnt= int4net(u.tzhead.tzh_leapcnt);
    sp->timecnt= int4net(u.tzhead.tzh_timecnt);
    sp->typecnt= int4net(u.tzhead.tzh_typecnt);
    sp->charcnt= int4net(u.tzhead.tzh_charcnt);
    p= u.tzhead.tzh_charcnt + sizeof(u.tzhead.tzh_charcnt);
    if (sp->leapcnt > TZ_MAX_LEAPS ||
        sp->typecnt == 0 || sp->typecnt > TZ_MAX_TYPES ||
        sp->timecnt > TZ_MAX_TIMES ||
        sp->charcnt > TZ_MAX_CHARS ||
        (ttisstdcnt != sp->typecnt && ttisstdcnt != 0) ||
        (ttisgmtcnt != sp->typecnt && ttisgmtcnt != 0))
      return 1;
    if ((uint)(read_from_file - (p - u.buf)) <
        sp->timecnt * 4 +                       /* ats */
        sp->timecnt +                           /* types */
        sp->typecnt * (4 + 2) +                 /* ttinfos */
        sp->charcnt +                           /* chars */
        sp->leapcnt * (4 + 4) +                 /* lsinfos */
        ttisstdcnt +                            /* ttisstds */
        ttisgmtcnt)                             /* ttisgmts */
      return 1;

    if (!(tzinfo_buf= (char *)alloc_root(storage,
                                         ALIGN_SIZE(sp->timecnt *
                                                    sizeof(my_time_t)) +
                                         ALIGN_SIZE(sp->timecnt) +
                                         ALIGN_SIZE(sp->typecnt *
                                                    sizeof(TRAN_TYPE_INFO)) +
#ifdef ABBR_ARE_USED
                                         ALIGN_SIZE(sp->charcnt) +
#endif
                                         sp->leapcnt * sizeof(LS_INFO))))
      return 1;

    sp->ats= (my_time_t *)tzinfo_buf;
    tzinfo_buf+= ALIGN_SIZE(sp->timecnt * sizeof(my_time_t));
    sp->types= (uchar *)tzinfo_buf;
    tzinfo_buf+= ALIGN_SIZE(sp->timecnt);
    sp->ttis= (TRAN_TYPE_INFO *)tzinfo_buf;
    tzinfo_buf+= ALIGN_SIZE(sp->typecnt * sizeof(TRAN_TYPE_INFO));
#ifdef ABBR_ARE_USED
    sp->chars= tzinfo_buf;
    tzinfo_buf+= ALIGN_SIZE(sp->charcnt);
#endif
    sp->lsis= (LS_INFO *)tzinfo_buf;

    for (i= 0; i < sp->timecnt; i++, p+= 4)
      sp->ats[i]= int4net(p);

    for (i= 0; i < sp->timecnt; i++)
    {
      sp->types[i]= (uchar) *p++;
      if (sp->types[i] >= sp->typecnt)
        return 1;
    }
    for (i= 0; i < sp->typecnt; i++)
    {
      TRAN_TYPE_INFO * ttisp;

      ttisp= &sp->ttis[i];
      ttisp->tt_gmtoff= int4net(p);
      p+= 4;
      ttisp->tt_isdst= (uchar) *p++;
      if (ttisp->tt_isdst != 0 && ttisp->tt_isdst != 1)
        return 1;
      ttisp->tt_abbrind= (uchar) *p++;
      if (ttisp->tt_abbrind > sp->charcnt)
        return 1;
    }
    for (i= 0; i < sp->charcnt; i++)
      sp->chars[i]= *p++;
    sp->chars[i]= '\0';	/* ensure '\0' at end */
    for (i= 0; i < sp->leapcnt; i++)
    {
      LS_INFO *lsisp;

      lsisp= &sp->lsis[i];
      lsisp->ls_trans= int4net(p);
      p+= 4;
      lsisp->ls_corr= int4net(p);
      p+= 4;
    }
    /*
      Since we don't support POSIX style TZ definitions in variables we
      don't read further like glibc or elsie code.
    */
  }

  return prepare_tz_info(sp, storage);
}
#endif /* defined(TZINFO2SQL) || defined(TESTTIME) */


/*
  Finish preparation of time zone description for use in TIME_to_gmt_sec()
  and gmt_sec_to_TIME() functions.

  SYNOPSIS
    prepare_tz_info()
      sp - pointer to time zone description
      storage - pointer to MEM_ROOT where arrays for map allocated

  DESCRIPTION
    First task of this function is to find fallback time type which will
    be used if there are no transitions or we have moment in time before
    any transitions.
    Second task is to build "shifted my_time_t" -> my_time_t map used in
    MYSQL_TIME -> my_time_t conversion.
    Note: See description of TIME_to_gmt_sec() function first.
    In order to perform MYSQL_TIME -> my_time_t conversion we need to build table
    which defines "shifted by tz offset and leap seconds my_time_t" ->
    my_time_t function wich is almost the same (except ranges of ambiguity)
    as reverse function to piecewise linear function used for my_time_t ->
    "shifted my_time_t" conversion and which is also specified as table in
    zoneinfo file or in our db (It is specified as start of time type ranges
    and time type offsets). So basic idea is very simple - let us iterate
    through my_time_t space from one point of discontinuity of my_time_t ->
    "shifted my_time_t" function to another and build our approximation of
    reverse function. (Actually we iterate through ranges on which
    my_time_t -> "shifted my_time_t" is linear function).

  RETURN VALUES
    0	Ok
    1	Error
*/
static my_bool
prepare_tz_info(TIME_ZONE_INFO *sp, MEM_ROOT *storage)
{
  my_time_t cur_t= MY_TIME_T_MIN;
  my_time_t cur_l, end_t, end_l;
  my_time_t cur_max_seen_l= MY_TIME_T_MIN;
  long cur_offset, cur_corr, cur_off_and_corr;
  uint next_trans_idx, next_leap_idx;
  uint i;
  /*
    Temporary arrays where we will store tables. Needed because
    we don't know table sizes ahead. (Well we can estimate their
    upper bound but this will take extra space.)
  */
  my_time_t revts[TZ_MAX_REV_RANGES];
  REVT_INFO revtis[TZ_MAX_REV_RANGES];

  LINT_INIT(end_l);

  /*
    Let us setup fallback time type which will be used if we have not any
    transitions or if we have moment of time before first transition.
    We will find first non-DST local time type and use it (or use first
    local time type if all of them are DST types).
  */
  for (i= 0; i < sp->typecnt && sp->ttis[i].tt_isdst; i++)
    /* no-op */ ;
  if (i == sp->typecnt)
    i= 0;
  sp->fallback_tti= &(sp->ttis[i]);


  /*
    Let us build shifted my_time_t -> my_time_t map.
  */
  sp->revcnt= 0;

  /* Let us find initial offset */
  if (sp->timecnt == 0 || cur_t < sp->ats[0])
  {
    /*
      If we have not any transitions or t is before first transition we are using
      already found fallback time type which index is already in i.
    */
    next_trans_idx= 0;
  }
  else
  {
    /* cur_t == sp->ats[0] so we found transition */
    i= sp->types[0];
    next_trans_idx= 1;
  }

  cur_offset= sp->ttis[i].tt_gmtoff;


  /* let us find leap correction... unprobable, but... */
  for (next_leap_idx= 0; next_leap_idx < sp->leapcnt &&
         cur_t >= sp->lsis[next_leap_idx].ls_trans;
         ++next_leap_idx)
    continue;

  if (next_leap_idx > 0)
    cur_corr= sp->lsis[next_leap_idx - 1].ls_corr;
  else
    cur_corr= 0;

  /* Iterate trough t space */
  while (sp->revcnt < TZ_MAX_REV_RANGES - 1)
  {
    cur_off_and_corr= cur_offset - cur_corr;

    /*
      We assuming that cur_t could be only overflowed downwards,
      we also assume that end_t won't be overflowed in this case.
    */
    if (cur_off_and_corr < 0 &&
        cur_t < MY_TIME_T_MIN - cur_off_and_corr)
      cur_t= MY_TIME_T_MIN - cur_off_and_corr;

    cur_l= cur_t + cur_off_and_corr;

    /*
      Let us choose end_t as point before next time type change or leap
      second correction.
    */
    end_t= min((next_trans_idx < sp->timecnt) ? sp->ats[next_trans_idx] - 1:
                                                MY_TIME_T_MAX,
               (next_leap_idx < sp->leapcnt) ?
                 sp->lsis[next_leap_idx].ls_trans - 1: MY_TIME_T_MAX);
    /*
      again assuming that end_t can be overlowed only in positive side
      we also assume that end_t won't be overflowed in this case.
    */
    if (cur_off_and_corr > 0 &&
        end_t > MY_TIME_T_MAX - cur_off_and_corr)
      end_t= MY_TIME_T_MAX - cur_off_and_corr;

    end_l= end_t + cur_off_and_corr;


    if (end_l > cur_max_seen_l)
    {
      /* We want special handling in the case of first range */
      if (cur_max_seen_l == MY_TIME_T_MIN)
      {
        revts[sp->revcnt]= cur_l;
        revtis[sp->revcnt].rt_offset= cur_off_and_corr;
        revtis[sp->revcnt].rt_type= 0;
        sp->revcnt++;
        cur_max_seen_l= end_l;
      }
      else
      {
        if (cur_l > cur_max_seen_l + 1)
        {
          /* We have a spring time-gap and we are not at the first range */
          revts[sp->revcnt]= cur_max_seen_l + 1;
          revtis[sp->revcnt].rt_offset= revtis[sp->revcnt-1].rt_offset;
          revtis[sp->revcnt].rt_type= 1;
          sp->revcnt++;
          if (sp->revcnt == TZ_MAX_TIMES + TZ_MAX_LEAPS + 1)
            break; /* That was too much */
          cur_max_seen_l= cur_l - 1;
        }

        /* Assume here end_l > cur_max_seen_l (because end_l>=cur_l) */

        revts[sp->revcnt]= cur_max_seen_l + 1;
        revtis[sp->revcnt].rt_offset= cur_off_and_corr;
        revtis[sp->revcnt].rt_type= 0;
        sp->revcnt++;
        cur_max_seen_l= end_l;
      }
    }

    if (end_t == MY_TIME_T_MAX ||
        (cur_off_and_corr > 0) &&
        (end_t >= MY_TIME_T_MAX - cur_off_and_corr))
      /* end of t space */
      break;

    cur_t= end_t + 1;

    /*
      Let us find new offset and correction. Because of our choice of end_t
      cur_t can only be point where new time type starts or/and leap
      correction is performed.
    */
    if (sp->timecnt != 0 && cur_t >= sp->ats[0]) /* else reuse old offset */
      if (next_trans_idx < sp->timecnt &&
          cur_t == sp->ats[next_trans_idx])
      {
        /* We are at offset point */
        cur_offset= sp->ttis[sp->types[next_trans_idx]].tt_gmtoff;
        ++next_trans_idx;
      }

    if (next_leap_idx < sp->leapcnt &&
        cur_t == sp->lsis[next_leap_idx].ls_trans)
    {
      /* we are at leap point */
      cur_corr= sp->lsis[next_leap_idx].ls_corr;
      ++next_leap_idx;
    }
  }

  /* check if we have had enough space */
  if (sp->revcnt == TZ_MAX_REV_RANGES - 1)
    return 1;

  /* set maximum end_l as finisher */
  revts[sp->revcnt]= end_l;

  /* Allocate arrays of proper size in sp and copy result there */
  if (!(sp->revts= (my_time_t *)alloc_root(storage,
                                  sizeof(my_time_t) * (sp->revcnt + 1))) ||
      !(sp->revtis= (REVT_INFO *)alloc_root(storage,
                                  sizeof(REVT_INFO) * sp->revcnt)))
    return 1;

  memcpy(sp->revts, revts, sizeof(my_time_t) * (sp->revcnt + 1));
  memcpy(sp->revtis, revtis, sizeof(REVT_INFO) * sp->revcnt);

  return 0;
}


#if !defined(TZINFO2SQL)

static const uint mon_lengths[2][MONS_PER_YEAR]=
{
  { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 },
  { 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 }
};

static const uint mon_starts[2][MONS_PER_YEAR]=
{
  { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 },
  { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335 }
};

static const uint year_lengths[2]=
{
  DAYS_PER_NYEAR, DAYS_PER_LYEAR
};

#define LEAPS_THRU_END_OF(y)  ((y) / 4 - (y) / 100 + (y) / 400)


/*
  Converts time from my_time_t representation (seconds in UTC since Epoch)
  to broken down representation using given local time zone offset.

  SYNOPSIS
    sec_to_TIME()
      tmp    - pointer to structure for broken down representation
      t      - my_time_t value to be converted
      offset - local time zone offset

  DESCRIPTION
    Convert my_time_t with offset to MYSQL_TIME struct. Differs from timesub
    (from elsie code) because doesn't contain any leap correction and
    TM_GMTOFF and is_dst setting and contains some MySQL specific
    initialization. Funny but with removing of these we almost have
    glibc's offtime function.
*/
static void
sec_to_TIME(MYSQL_TIME * tmp, my_time_t t, long offset)
{
  long days;
  long rem;
  int y;
  int yleap;
  const uint *ip;

  days= (long) (t / SECS_PER_DAY);
  rem=  (long) (t % SECS_PER_DAY);

  /*
    We do this as separate step after dividing t, because this
    allows us handle times near my_time_t bounds without overflows.
  */
  rem+= offset;
  while (rem < 0)
  {
    rem+= SECS_PER_DAY;
    days--;
  }
  while (rem >= SECS_PER_DAY)
  {
    rem -= SECS_PER_DAY;
    days++;
  }
  tmp->hour= (uint)(rem / SECS_PER_HOUR);
  rem= rem % SECS_PER_HOUR;
  tmp->minute= (uint)(rem / SECS_PER_MIN);
  /*
    A positive leap second requires a special
    representation.  This uses "... ??:59:60" et seq.
  */
  tmp->second= (uint)(rem % SECS_PER_MIN);

  y= EPOCH_YEAR;
  while (days < 0 || days >= (long)year_lengths[yleap= isleap(y)])
  {
    int	newy;

    newy= y + days / DAYS_PER_NYEAR;
    if (days < 0)
      newy--;
    days-= (newy - y) * DAYS_PER_NYEAR +
           LEAPS_THRU_END_OF(newy - 1) -
           LEAPS_THRU_END_OF(y - 1);
    y= newy;
  }
  tmp->year= y;

  ip= mon_lengths[yleap];
  for (tmp->month= 0; days >= (long) ip[tmp->month]; tmp->month++)
    days= days - (long) ip[tmp->month];
  tmp->month++;
  tmp->day= (uint)(days + 1);

  /* filling MySQL specific MYSQL_TIME members */
  tmp->neg= 0; tmp->second_part= 0;
  tmp->time_type= MYSQL_TIMESTAMP_DATETIME;
}


/*
  Find time range wich contains given my_time_t value

  SYNOPSIS
    find_time_range()
      t                - my_time_t value for which we looking for range
      range_boundaries - sorted array of range starts.
      higher_bound     - number of ranges

  DESCRIPTION
    Performs binary search for range which contains given my_time_t value.
    It has sense if number of ranges is greater than zero and my_time_t value
    is greater or equal than beginning of first range. It also assumes that
    t belongs to some range specified or end of last is MY_TIME_T_MAX.

    With this localtime_r on real data may takes less time than with linear
    search (I've seen 30% speed up).

  RETURN VALUE
    Index of range to which t belongs
*/
static uint
find_time_range(my_time_t t, const my_time_t *range_boundaries,
                uint higher_bound)
{
  uint i, lower_bound= 0;

  /*
    Function will work without this assertion but result would be meaningless.
  */
  DBUG_ASSERT(higher_bound > 0 && t >= range_boundaries[0]);

  /*
    Do binary search for minimal interval which contain t. We preserve:
    range_boundaries[lower_bound] <= t < range_boundaries[higher_bound]
    invariant and decrease this higher_bound - lower_bound gap twice
    times on each step.
  */

  while (higher_bound - lower_bound > 1)
  {
    i= (lower_bound + higher_bound) >> 1;
    if (range_boundaries[i] <= t)
      lower_bound= i;
    else
      higher_bound= i;
  }
  return lower_bound;
}

/*
  Find local time transition for given my_time_t.

  SYNOPSIS
    find_transition_type()
      t   - my_time_t value to be converted
      sp  - pointer to struct with time zone description

  RETURN VALUE
    Pointer to structure in time zone description describing
    local time type for given my_time_t.
*/
static
const TRAN_TYPE_INFO *
find_transition_type(my_time_t t, const TIME_ZONE_INFO *sp)
{
  if (unlikely(sp->timecnt == 0 || t < sp->ats[0]))
  {
    /*
      If we have not any transitions or t is before first transition let
      us use fallback time type.
    */
    return sp->fallback_tti;
  }

  /*
    Do binary search for minimal interval between transitions which
    contain t. With this localtime_r on real data may takes less
    time than with linear search (I've seen 30% speed up).
  */
  return &(sp->ttis[sp->types[find_time_range(t, sp->ats, sp->timecnt)]]);
}


/*
  Converts time in my_time_t representation (seconds in UTC since Epoch) to
  broken down MYSQL_TIME representation in local time zone.

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp          - pointer to structure for broken down represenatation
      sec_in_utc   - my_time_t value to be converted
      sp           - pointer to struct with time zone description

  TODO
    We can improve this function by creating joined array of transitions and
    leap corrections. This will require adding extra field to TRAN_TYPE_INFO
    for storing number of "extra" seconds to minute occured due to correction
    (60th and 61st second, look how we calculate them as "hit" in this
    function).
    Under realistic assumptions about frequency of transitions the same array
    can be used fot MYSQL_TIME -> my_time_t conversion. For this we need to
    implement tweaked binary search which will take into account that some
    MYSQL_TIME has two matching my_time_t ranges and some of them have none.
*/
static void
gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t sec_in_utc, const TIME_ZONE_INFO *sp)
{
  const TRAN_TYPE_INFO *ttisp;
  const LS_INFO *lp;
  long  corr= 0;
  int   hit= 0;
  int   i;

  /*
    Find proper transition (and its local time type) for our sec_in_utc value.
    Funny but again by separating this step in function we receive code
    which very close to glibc's code. No wonder since they obviously use
    the same base and all steps are sensible.
  */
  ttisp= find_transition_type(sec_in_utc, sp);

  /*
    Let us find leap correction for our sec_in_utc value and number of extra
    secs to add to this minute.
    This loop is rarely used because most users will use time zones without
    leap seconds, and even in case when we have such time zone there won't
    be many iterations (we have about 22 corrections at this moment (2004)).
  */
  for ( i= sp->leapcnt; i-- > 0; )
  {
    lp= &sp->lsis[i];
    if (sec_in_utc >= lp->ls_trans)
    {
      if (sec_in_utc == lp->ls_trans)
      {
        hit= ((i == 0 && lp->ls_corr > 0) ||
              lp->ls_corr > sp->lsis[i - 1].ls_corr);
        if (hit)
        {
          while (i > 0 &&
                 sp->lsis[i].ls_trans == sp->lsis[i - 1].ls_trans + 1 &&
                 sp->lsis[i].ls_corr == sp->lsis[i - 1].ls_corr + 1)
          {
            hit++;
            i--;
          }
        }
      }
      corr= lp->ls_corr;
      break;
    }
  }

  sec_to_TIME(tmp, sec_in_utc, ttisp->tt_gmtoff - corr);

  tmp->second+= hit;
}


/*
  Converts local time in broken down representation to local
  time zone analog of my_time_t represenation.

  SYNOPSIS
    sec_since_epoch()
      year, mon, mday, hour, min, sec - broken down representation.

  DESCRIPTION
    Converts time in broken down representation to my_time_t representation
    ignoring time zone. Note that we cannot convert back some valid _local_
    times near ends of my_time_t range because of my_time_t  overflow. But we
    ignore this fact now since MySQL will never pass such argument.

  RETURN VALUE
    Seconds since epoch time representation.
*/
static my_time_t
sec_since_epoch(int year, int mon, int mday, int hour, int min ,int sec)
{
  /* Guard against my_time_t overflow(on system with 32 bit my_time_t) */
  DBUG_ASSERT(!(year == TIMESTAMP_MAX_YEAR && mon == 1 && mday > 17));
#ifndef WE_WANT_TO_HANDLE_UNORMALIZED_DATES
  /*
    It turns out that only whenever month is normalized or unnormalized
    plays role.
  */
  DBUG_ASSERT(mon > 0 && mon < 13);
  long days= year * DAYS_PER_NYEAR - EPOCH_YEAR * DAYS_PER_NYEAR +
             LEAPS_THRU_END_OF(year - 1) -
             LEAPS_THRU_END_OF(EPOCH_YEAR - 1);
  days+= mon_starts[isleap(year)][mon - 1];
#else
  long norm_month= (mon - 1) % MONS_PER_YEAR;
  long a_year= year + (mon - 1)/MONS_PER_YEAR - (int)(norm_month < 0);
  long days= a_year * DAYS_PER_NYEAR - EPOCH_YEAR * DAYS_PER_NYEAR +
             LEAPS_THRU_END_OF(a_year - 1) -
             LEAPS_THRU_END_OF(EPOCH_YEAR - 1);
  days+= mon_starts[isleap(a_year)]
                    [norm_month + (norm_month < 0 ? MONS_PER_YEAR : 0)];
#endif
  days+= mday - 1;

  return ((days * HOURS_PER_DAY + hour) * MINS_PER_HOUR + min) *
         SECS_PER_MIN + sec;
}

/*
  Converts local time in broken down MYSQL_TIME representation to my_time_t
  representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to structure for broken down represenatation
      sp              - pointer to struct with time zone description
      in_dst_time_gap - pointer to bool which is set to true if datetime
                        value passed doesn't really exist (i.e. falls into
                        spring time-gap) and is not touched otherwise.

  DESCRIPTION
    This is mktime analog for MySQL. It is essentially different
    from mktime (or hypotetical my_mktime) because:
    - It has no idea about tm_isdst member so if it
      has two answers it will give the smaller one
    - If we are in spring time gap then it will return
      beginning of the gap
    - It can give wrong results near the ends of my_time_t due to
      overflows, but we are safe since in MySQL we will never
      call this function for such dates (its restriction for year
      between 1970 and 2038 gives us several days of reserve).
    - By default it doesn't support un-normalized input. But if
      sec_since_epoch() function supports un-normalized dates
      then this function should handle un-normalized input right,
      altough it won't normalize structure TIME.

    Traditional approach to problem of conversion from broken down
    representation to time_t is iterative. Both elsie's and glibc
    implementation try to guess what time_t value should correspond to
    this broken-down value. They perform localtime_r function on their
    guessed value and then calculate the difference and try to improve
    their guess. Elsie's code guesses time_t value in bit by bit manner,
    Glibc's code tries to add difference between broken-down value
    corresponding to guess and target broken-down value to current guess.
    It also uses caching of last found correction... So Glibc's approach
    is essentially faster but introduces some undetermenism (in case if
    is_dst member of broken-down representation (tm struct) is not known
    and we have two possible answers).

    We use completely different approach. It is better since it is both
    faster than iterative implementations and fully determenistic. If you
    look at my_time_t to MYSQL_TIME conversion then you'll find that it consist
    of two steps:
    The first is calculating shifted my_time_t value and the second - TIME
    calculation from shifted my_time_t value (well it is a bit simplified
    picture). The part in which we are interested in is my_time_t -> shifted
    my_time_t conversion. It is piecewise linear function which is defined
    by combination of transition times as break points and times offset
    as changing function parameter. The possible inverse function for this
    converison would be ambiguos but with MySQL's restrictions we can use
    some function which is the same as inverse function on unambigiuos
    ranges and coincides with one of branches of inverse function in
    other ranges. Thus we just need to build table which will determine
    this shifted my_time_t -> my_time_t conversion similar to existing
    (my_time_t -> shifted my_time_t table). We do this in
    prepare_tz_info function.

  TODO
    If we can even more improve this function. For doing this we will need to
    build joined map of transitions and leap corrections for gmt_sec_to_TIME()
    function (similar to revts/revtis). Under realistic assumptions about
    frequency of transitions we can use the same array for TIME_to_gmt_sec().
    We need to implement special version of binary search for this. Such step
    will be beneficial to CPU cache since we will decrease data-set used for
    conversion twice.

  RETURN VALUE
    Seconds in UTC since Epoch.
    0 in case of error.
*/
static my_time_t
TIME_to_gmt_sec(const MYSQL_TIME *t, const TIME_ZONE_INFO *sp,
                my_bool *in_dst_time_gap)
{
  my_time_t local_t;
  uint saved_seconds;
  uint i;
  int shift= 0;

  DBUG_ENTER("TIME_to_gmt_sec");

  if (!validate_timestamp_range(t))
    DBUG_RETURN(0);


  /* We need this for correct leap seconds handling */
  if (t->second < SECS_PER_MIN)
    saved_seconds= 0;
  else
    saved_seconds= t->second;

  /*
    NOTE: to convert full my_time_t range we do a shift of the
    boundary dates here to avoid overflow of my_time_t.
    We use alike approach in my_system_gmt_sec().

    However in that function we also have to take into account
    overflow near 0 on some platforms. That's because my_system_gmt_sec
    uses localtime_r(), which doesn't work with negative values correctly
    on platforms with unsigned time_t (QNX). Here we don't use localtime()
    => we negative values of local_t are ok.
  */

  if ((t->year == TIMESTAMP_MAX_YEAR) && (t->month == 1) && t->day > 4)
  {
    /*
      We will pass (t->day - shift) to sec_since_epoch(), and
      want this value to be a positive number, so we shift
      only dates > 4.01.2038 (to avoid owerflow).
    */
    shift= 2;
  }


  local_t= sec_since_epoch(t->year, t->month, (t->day - shift),
                           t->hour, t->minute,
                           saved_seconds ? 0 : t->second);

  /* We have at least one range */
  DBUG_ASSERT(sp->revcnt >= 1);

  if (local_t < sp->revts[0] || local_t > sp->revts[sp->revcnt])
  {
    /*
      This means that source time can't be represented as my_time_t due to
      limited my_time_t range.
    */
    DBUG_RETURN(0);
  }

  /* binary search for our range */
  i= find_time_range(local_t, sp->revts, sp->revcnt);

  /*
    As there are no offset switches at the end of TIMESTAMP range,
    we could simply check for overflow here (and don't need to bother
    about DST gaps etc)
  */
  if (shift)
  {
    if (local_t > (my_time_t) (TIMESTAMP_MAX_VALUE - shift * SECS_PER_DAY +
                               sp->revtis[i].rt_offset - saved_seconds))
    {
      DBUG_RETURN(0);                           /* my_time_t overflow */
    }
    local_t+= shift * SECS_PER_DAY;
  }

  if (sp->revtis[i].rt_type)
  {
    /*
      Oops! We are in spring time gap.
      May be we should return error here?
      Now we are returning my_time_t value corresponding to the
      beginning of the gap.
    */
    *in_dst_time_gap= 1;
    local_t= sp->revts[i] - sp->revtis[i].rt_offset + saved_seconds;
  }
  else
    local_t= local_t - sp->revtis[i].rt_offset + saved_seconds;

  /* check for TIMESTAMP_MAX_VALUE was already done above */
  if (local_t < TIMESTAMP_MIN_VALUE)
    local_t= 0;

  DBUG_RETURN(local_t);
}


/*
  End of elsie derived code.
*/
#endif /* !defined(TZINFO2SQL) */


#if !defined(TESTTIME) && !defined(TZINFO2SQL)

/*
  String with names of SYSTEM time zone.
*/
static const String tz_SYSTEM_name("SYSTEM", 6, &my_charset_latin1);


/*
  Instance of this class represents local time zone used on this system
  (specified by TZ environment variable or via any other system mechanism).
  It uses system functions (localtime_r, my_system_gmt_sec) for conversion
  and is always available. Because of this it is used by default - if there
  were no explicit time zone specified. On the other hand because of this
  conversion methods provided by this class is significantly slower and
  possibly less multi-threaded-friendly than corresponding Time_zone_db
  methods so the latter should be preffered there it is possible.
*/
class Time_zone_system : public Time_zone
{
public:
  Time_zone_system() {}                       /* Remove gcc warning */
  virtual my_time_t TIME_to_gmt_sec(const MYSQL_TIME *t,
                                    my_bool *in_dst_time_gap) const;
  virtual void gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const;
  virtual const String * get_name() const;
};


/*
  Converts local time in system time zone in MYSQL_TIME representation
  to its my_time_t representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to MYSQL_TIME structure with local time in
                        broken-down representation.
      in_dst_time_gap - pointer to bool which is set to true if datetime
                        value passed doesn't really exist (i.e. falls into
                        spring time-gap) and is not touched otherwise.

  DESCRIPTION
    This method uses system function (localtime_r()) for conversion
    local time in system time zone in MYSQL_TIME structure to its my_time_t
    representation. Unlike the same function for Time_zone_db class
    it it won't handle unnormalized input properly. Still it will
    return lowest possible my_time_t in case of ambiguity or if we
    provide time corresponding to the time-gap.

    You should call init_time() function before using this function.

  RETURN VALUE
    Corresponding my_time_t value or 0 in case of error
*/
my_time_t
Time_zone_system::TIME_to_gmt_sec(const MYSQL_TIME *t, my_bool *in_dst_time_gap) const
{
  long not_used;
  return my_system_gmt_sec(t, &not_used, in_dst_time_gap);
}


/*
  Converts time from UTC seconds since Epoch (my_time_t) representation
  to system local time zone broken-down representation.

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp - pointer to MYSQL_TIME structure to fill-in
      t   - my_time_t value to be converted

  NOTE
    We assume that value passed to this function will fit into time_t range
    supported by localtime_r. This conversion is putting restriction on
    TIMESTAMP range in MySQL. If we can get rid of SYSTEM time zone at least
    for interaction with client then we can extend TIMESTAMP range down to
    the 1902 easily.
*/
void
Time_zone_system::gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const
{
  struct tm tmp_tm;
  time_t tmp_t= (time_t)t;

  localtime_r(&tmp_t, &tmp_tm);
  localtime_to_TIME(tmp, &tmp_tm);
  tmp->time_type= MYSQL_TIMESTAMP_DATETIME;
}


/*
  Get name of time zone

  SYNOPSIS
    get_name()

  RETURN VALUE
    Name of time zone as String
*/
const String *
Time_zone_system::get_name() const
{
  return &tz_SYSTEM_name;
}


/*
  Instance of this class represents UTC time zone. It uses system gmtime_r
  function for conversions and is always available. It is used only for
  my_time_t -> MYSQL_TIME conversions in various UTC_...  functions, it is not
  intended for MYSQL_TIME -> my_time_t conversions and shouldn't be exposed to user.
*/
class Time_zone_utc : public Time_zone
{
public:
  Time_zone_utc() {}                          /* Remove gcc warning */
  virtual my_time_t TIME_to_gmt_sec(const MYSQL_TIME *t,
                                    my_bool *in_dst_time_gap) const;
  virtual void gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const;
  virtual const String * get_name() const;
};


/*
  Convert UTC time from MYSQL_TIME representation to its my_time_t representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to MYSQL_TIME structure with local time
                        in broken-down representation.
      in_dst_time_gap - pointer to bool which is set to true if datetime
                        value passed doesn't really exist (i.e. falls into
                        spring time-gap) and is not touched otherwise.

  DESCRIPTION
    Since Time_zone_utc is used only internally for my_time_t -> TIME
    conversions, this function of Time_zone interface is not implemented for
    this class and should not be called.

  RETURN VALUE
    0
*/
my_time_t
Time_zone_utc::TIME_to_gmt_sec(const MYSQL_TIME *t, my_bool *in_dst_time_gap) const
{
  /* Should be never called */
  DBUG_ASSERT(0);
  return 0;
}


/*
  Converts time from UTC seconds since Epoch (my_time_t) representation
  to broken-down representation (also in UTC).

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp - pointer to MYSQL_TIME structure to fill-in
      t   - my_time_t value to be converted

  NOTE
    See note for apropriate Time_zone_system method.
*/
void
Time_zone_utc::gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const
{
  struct tm tmp_tm;
  time_t tmp_t= (time_t)t;
  gmtime_r(&tmp_t, &tmp_tm);
  localtime_to_TIME(tmp, &tmp_tm);
  tmp->time_type= MYSQL_TIMESTAMP_DATETIME;
}


/*
  Get name of time zone

  SYNOPSIS
    get_name()

  DESCRIPTION
    Since Time_zone_utc is used only internally by SQL's UTC_* functions it
    is not accessible directly, and hence this function of Time_zone
    interface is not implemented for this class and should not be called.

  RETURN VALUE
    0
*/
const String *
Time_zone_utc::get_name() const
{
  /* Should be never called */
  DBUG_ASSERT(0);
  return 0;
}


/*
  Instance of this class represents some time zone which is
  described in mysql.time_zone family of tables.
*/
class Time_zone_db : public Time_zone
{
public:
  Time_zone_db(TIME_ZONE_INFO *tz_info_arg, const String * tz_name_arg);
  virtual my_time_t TIME_to_gmt_sec(const MYSQL_TIME *t,
                                    my_bool *in_dst_time_gap) const;
  virtual void gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const;
  virtual const String * get_name() const;
private:
  TIME_ZONE_INFO *tz_info;
  const String *tz_name;
};


/*
  Initializes object representing time zone described by mysql.time_zone
  tables.

  SYNOPSIS
    Time_zone_db()
      tz_info_arg - pointer to TIME_ZONE_INFO structure which is filled
                    according to db or other time zone description
                    (for example by my_tz_init()).
                    Several Time_zone_db instances can share one
                    TIME_ZONE_INFO structure.
      tz_name_arg - name of time zone.
*/
Time_zone_db::Time_zone_db(TIME_ZONE_INFO *tz_info_arg,
                           const String *tz_name_arg):
  tz_info(tz_info_arg), tz_name(tz_name_arg)
{
}


/*
  Converts local time in time zone described from TIME
  representation to its my_time_t representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to MYSQL_TIME structure with local time
                        in broken-down representation.
      in_dst_time_gap - pointer to bool which is set to true if datetime
                        value passed doesn't really exist (i.e. falls into
                        spring time-gap) and is not touched otherwise.

  DESCRIPTION
    Please see ::TIME_to_gmt_sec for function description and
    parameter restrictions.

  RETURN VALUE
    Corresponding my_time_t value or 0 in case of error
*/
my_time_t
Time_zone_db::TIME_to_gmt_sec(const MYSQL_TIME *t, my_bool *in_dst_time_gap) const
{
  return ::TIME_to_gmt_sec(t, tz_info, in_dst_time_gap);
}


/*
  Converts time from UTC seconds since Epoch (my_time_t) representation
  to local time zone described in broken-down representation.

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp - pointer to MYSQL_TIME structure to fill-in
      t   - my_time_t value to be converted
*/
void
Time_zone_db::gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const
{
  ::gmt_sec_to_TIME(tmp, t, tz_info);
}


/*
  Get name of time zone

  SYNOPSIS
    get_name()

  RETURN VALUE
    Name of time zone as ASCIIZ-string
*/
const String *
Time_zone_db::get_name() const
{
  return tz_name;
}


/*
  Instance of this class represents time zone which
  was specified as offset from UTC.
*/
class Time_zone_offset : public Time_zone
{
public:
  Time_zone_offset(long tz_offset_arg);
  virtual my_time_t TIME_to_gmt_sec(const MYSQL_TIME *t,
                                    my_bool *in_dst_time_gap) const;
  virtual void   gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const;
  virtual const String * get_name() const;
  /*
    This have to be public because we want to be able to access it from
    my_offset_tzs_get_key() function
  */
  long offset;
private:
  /* Extra reserve because of snprintf */
  char name_buff[7+16];
  String name;
};


/*
  Initializes object representing time zone described by its offset from UTC.

  SYNOPSIS
    Time_zone_offset()
      tz_offset_arg - offset from UTC in seconds.
                      Positive for direction to east.
*/
Time_zone_offset::Time_zone_offset(long tz_offset_arg):
  offset(tz_offset_arg)
{
  uint hours= abs((int)(offset / SECS_PER_HOUR));
  uint minutes= abs((int)(offset % SECS_PER_HOUR / SECS_PER_MIN));
  ulong length= my_snprintf(name_buff, sizeof(name_buff), "%s%02d:%02d",
                            (offset>=0) ? "+" : "-", hours, minutes);
  name.set(name_buff, length, &my_charset_latin1);
}


/*
  Converts local time in time zone described as offset from UTC
  from MYSQL_TIME representation to its my_time_t representation.

  SYNOPSIS
    TIME_to_gmt_sec()
      t               - pointer to MYSQL_TIME structure with local time
                        in broken-down representation.
      in_dst_time_gap - pointer to bool which should be set to true if
                        datetime  value passed doesn't really exist
                        (i.e. falls into spring time-gap) and is not
                        touched otherwise.
                        It is not really used in this class.

  RETURN VALUE
    Corresponding my_time_t value or 0 in case of error
*/
my_time_t
Time_zone_offset::TIME_to_gmt_sec(const MYSQL_TIME *t, my_bool *in_dst_time_gap) const
{
  my_time_t local_t;
  int shift= 0;

  /*
    Check timestamp range.we have to do this as calling function relies on
    us to make all validation checks here.
  */
  if (!validate_timestamp_range(t))
    return 0;

  /*
    Do a temporary shift of the boundary dates to avoid
    overflow of my_time_t if the time value is near it's
    maximum range
  */
  if ((t->year == TIMESTAMP_MAX_YEAR) && (t->month == 1) && t->day > 4)
    shift= 2;

  local_t= sec_since_epoch(t->year, t->month, (t->day - shift),
                           t->hour, t->minute, t->second) -
           offset;

  if (shift)
  {
    /* Add back the shifted time */
    local_t+= shift * SECS_PER_DAY;
  }

  if (local_t >= TIMESTAMP_MIN_VALUE && local_t <= TIMESTAMP_MAX_VALUE)
    return local_t;

  /* range error*/
  return 0;
}


/*
  Converts time from UTC seconds since Epoch (my_time_t) representation
  to local time zone described as offset from UTC and in broken-down
  representation.

  SYNOPSIS
    gmt_sec_to_TIME()
      tmp - pointer to MYSQL_TIME structure to fill-in
      t   - my_time_t value to be converted
*/
void
Time_zone_offset::gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const
{
  sec_to_TIME(tmp, t, offset);
}


/*
  Get name of time zone

  SYNOPSIS
    get_name()

  RETURN VALUE
    Name of time zone as pointer to String object
*/
const String *
Time_zone_offset::get_name() const
{
  return &name;
}


static Time_zone_utc tz_UTC;
static Time_zone_system tz_SYSTEM;
static Time_zone_offset tz_OFFSET0(0);

Time_zone *my_tz_OFFSET0= &tz_OFFSET0;
Time_zone *my_tz_UTC= &tz_UTC;
Time_zone *my_tz_SYSTEM= &tz_SYSTEM;

static HASH tz_names;
static HASH offset_tzs;
static MEM_ROOT tz_storage;

/*
  These mutex protects offset_tzs and tz_storage.
  These protection needed only when we are trying to set
  time zone which is specified as offset, and searching for existing
  time zone in offset_tzs or creating if it didn't existed before in
  tz_storage. So contention is low.
*/
static pthread_mutex_t tz_LOCK;
static bool tz_inited= 0;

/*
  This two static variables are inteded for holding info about leap seconds
  shared by all time zones.
*/
static uint tz_leapcnt= 0;
static LS_INFO *tz_lsis= 0;

/*
  Shows whenever we have found time zone tables during start-up.
  Used for avoiding of putting those tables to global table list
  for queries that use time zone info.
*/
static bool time_zone_tables_exist= 1;


/*
  Names of tables (with their lengths) that are needed
  for dynamical loading of time zone descriptions.
*/

static const LEX_STRING tz_tables_names[MY_TZ_TABLES_COUNT]=
{
  { C_STRING_WITH_LEN("time_zone_name")},
  { C_STRING_WITH_LEN("time_zone")},
  { C_STRING_WITH_LEN("time_zone_transition_type")},
  { C_STRING_WITH_LEN("time_zone_transition")}
};

/* Name of database to which those tables belong. */

static const LEX_STRING tz_tables_db_name= { C_STRING_WITH_LEN("mysql")};


class Tz_names_entry: public Sql_alloc
{
public:
  String name;
  Time_zone *tz;
};


/*
  We are going to call both of these functions from C code so
  they should obey C calling conventions.
*/

extern "C" uchar *
my_tz_names_get_key(Tz_names_entry *entry, size_t *length,
                    my_bool not_used __attribute__((unused)))
{
  *length= entry->name.length();
  return (uchar*) entry->name.ptr();
}

extern "C" uchar *
my_offset_tzs_get_key(Time_zone_offset *entry,
                      size_t *length,
                      my_bool not_used __attribute__((unused)))
{
  *length= sizeof(long);
  return (uchar*) &entry->offset;
}


/*
  Prepare table list with time zone related tables from preallocated array.

  SYNOPSIS
    tz_init_table_list()
      tz_tabs         - pointer to preallocated array of MY_TZ_TABLES_COUNT
                        TABLE_LIST objects

  DESCRIPTION
    This function prepares list of TABLE_LIST objects which can be used
    for opening of time zone tables from preallocated array.
*/

static void
tz_init_table_list(TABLE_LIST *tz_tabs)
{
  bzero(tz_tabs, sizeof(TABLE_LIST) * MY_TZ_TABLES_COUNT);

  for (int i= 0; i < MY_TZ_TABLES_COUNT; i++)
  {
    tz_tabs[i].alias= tz_tabs[i].table_name= tz_tables_names[i].str;
    tz_tabs[i].table_name_length= tz_tables_names[i].length;
    tz_tabs[i].db= tz_tables_db_name.str;
    tz_tabs[i].db_length= tz_tables_db_name.length;
    tz_tabs[i].lock_type= TL_READ;

    if (i != MY_TZ_TABLES_COUNT - 1)
      tz_tabs[i].next_global= tz_tabs[i].next_local= &tz_tabs[i+1];
    if (i != 0)
      tz_tabs[i].prev_global= &tz_tabs[i-1].next_global;
  }
}


/*
  Initialize time zone support infrastructure.

  SYNOPSIS
    my_tz_init()
      thd            - current thread object
      default_tzname - default time zone or 0 if none.
      bootstrap      - indicates whenever we are in bootstrap mode

  DESCRIPTION
    This function will init memory structures needed for time zone support,
    it will register mandatory SYSTEM time zone in them. It will try to open
    mysql.time_zone* tables and load information about default time zone and
    information which further will be shared among all time zones loaded.
    If system tables with time zone descriptions don't exist it won't fail
    (unless default_tzname is time zone from tables). If bootstrap parameter
    is true then this routine assumes that we are in bootstrap mode and won't
    load time zone descriptions unless someone specifies default time zone
    which is supposedly stored in those tables.
    It'll also set default time zone if it is specified.

  RETURN VALUES
    0 - ok
    1 - Error
*/
my_bool
my_tz_init(THD *org_thd, const char *default_tzname, my_bool bootstrap)
{
  THD *thd;
  TABLE_LIST tz_tables[1+MY_TZ_TABLES_COUNT];
  Open_tables_state open_tables_state_backup;
  TABLE *table;
  Tz_names_entry *tmp_tzname;
  my_bool return_val= 1;
  char db[]= "mysql";
  int res;
  DBUG_ENTER("my_tz_init");

  /*
    To be able to run this from boot, we allocate a temporary THD
  */
  if (!(thd= new THD))
    DBUG_RETURN(1);
  thd->thread_stack= (char*) &thd;
  thd->store_globals();
  lex_start(thd);

  /* Init all memory structures that require explicit destruction */
  if (hash_init(&tz_names, &my_charset_latin1, 20,
                0, 0, (hash_get_key) my_tz_names_get_key, 0, 0))
  {
    sql_print_error("Fatal error: OOM while initializing time zones");
    goto end;
  }
  if (hash_init(&offset_tzs, &my_charset_latin1, 26, 0, 0,
                (hash_get_key)my_offset_tzs_get_key, 0, 0))
  {
    sql_print_error("Fatal error: OOM while initializing time zones");
    hash_free(&tz_names);
    goto end;
  }
  init_alloc_root(&tz_storage, 32 * 1024, 0);
  VOID(pthread_mutex_init(&tz_LOCK, MY_MUTEX_INIT_FAST));
  tz_inited= 1;

  /* Add 'SYSTEM' time zone to tz_names hash */
  if (!(tmp_tzname= new (&tz_storage) Tz_names_entry()))
  {
    sql_print_error("Fatal error: OOM while initializing time zones");
    goto end_with_cleanup;
  }
  tmp_tzname->name.set(STRING_WITH_LEN("SYSTEM"), &my_charset_latin1);
  tmp_tzname->tz= my_tz_SYSTEM;
  if (my_hash_insert(&tz_names, (const uchar *)tmp_tzname))
  {
    sql_print_error("Fatal error: OOM while initializing time zones");
    goto end_with_cleanup;
  }

  if (bootstrap)
  {
    /* If we are in bootstrap mode we should not load time zone tables */
    return_val= time_zone_tables_exist= 0;
    goto end_with_setting_default_tz;
  }

  /*
    After this point all memory structures are inited and we even can live
    without time zone description tables. Now try to load information about
    leap seconds shared by all time zones.
  */

  thd->set_db(db, sizeof(db)-1);
  bzero((char*) &tz_tables[0], sizeof(TABLE_LIST));
  tz_tables[0].alias= tz_tables[0].table_name=
    (char*)"time_zone_leap_second";
  tz_tables[0].table_name_length= 21;
  tz_tables[0].db= db;
  tz_tables[0].db_length= sizeof(db)-1;
  tz_tables[0].lock_type= TL_READ;

  tz_init_table_list(tz_tables+1);
  tz_tables[0].next_global= tz_tables[0].next_local= &tz_tables[1];
  tz_tables[1].prev_global= &tz_tables[0].next_global;

  /*
    We need to open only mysql.time_zone_leap_second, but we try to
    open all time zone tables to see if they exist.
  */
  if (open_system_tables_for_read(thd, tz_tables, &open_tables_state_backup))
  {
    sql_print_warning("Can't open and lock time zone table: %s "
                      "trying to live without them", thd->net.last_error);
    /* We will try emulate that everything is ok */
    return_val= time_zone_tables_exist= 0;
    goto end_with_setting_default_tz;
  }

  /*
    Now we are going to load leap seconds descriptions that are shared
    between all time zones that use them. We are using index for getting
    records in proper order. Since we share the same MEM_ROOT between
    all time zones we just allocate enough memory for it first.
  */
  if (!(tz_lsis= (LS_INFO*) alloc_root(&tz_storage,
                                       sizeof(LS_INFO) * TZ_MAX_LEAPS)))
  {
    sql_print_error("Fatal error: Out of memory while loading "
                    "mysql.time_zone_leap_second table");
    goto end_with_close;
  }

  table= tz_tables[0].table;
  /*
    It is OK to ignore ha_index_init()/ha_index_end() return values since
    mysql.time_zone* tables are MyISAM and these operations always succeed
    for MyISAM.
  */
  (void)table->file->ha_index_init(0, 1);
  table->use_all_columns();

  tz_leapcnt= 0;

  res= table->file->index_first(table->record[0]);

  while (!res)
  {
    if (tz_leapcnt + 1 > TZ_MAX_LEAPS)
    {
      sql_print_error("Fatal error: While loading mysql.time_zone_leap_second"
                      " table: too much leaps");
      table->file->ha_index_end();
      goto end_with_close;
    }

    tz_lsis[tz_leapcnt].ls_trans= (my_time_t)table->field[0]->val_int();
    tz_lsis[tz_leapcnt].ls_corr= (long)table->field[1]->val_int();

    tz_leapcnt++;

    DBUG_PRINT("info",
               ("time_zone_leap_second table: tz_leapcnt: %u  tt_time: %lu  offset: %ld",
                tz_leapcnt, (ulong) tz_lsis[tz_leapcnt-1].ls_trans,
                tz_lsis[tz_leapcnt-1].ls_corr));

    res= table->file->index_next(table->record[0]);
  }

  (void)table->file->ha_index_end();

  if (res != HA_ERR_END_OF_FILE)
  {
    sql_print_error("Fatal error: Error while loading "
                    "mysql.time_zone_leap_second table");
    goto end_with_close;
  }

  /*
    Loading of info about leap seconds succeeded
  */

  return_val= 0;


end_with_setting_default_tz:
  /* If we have default time zone try to load it */
  if (default_tzname)
  {
    String tmp_tzname2(default_tzname, &my_charset_latin1);
    /*
      Time zone tables may be open here, and my_tz_find() may open
      most of them once more, but this is OK for system tables open
      for READ.
    */
    if (!(global_system_variables.time_zone= my_tz_find(thd, &tmp_tzname2)))
    {
      sql_print_error("Fatal error: Illegal or unknown default time zone '%s'",
                      default_tzname);
      return_val= 1;
    }
  }

end_with_close:
  if (time_zone_tables_exist)
  {
    thd->version--; /* Force close to free memory */
    close_system_tables(thd, &open_tables_state_backup);
  }

end_with_cleanup:

  /* if there were error free time zone describing structs */
  if (return_val)
    my_tz_free();
end:
  delete thd;
  if (org_thd)
    org_thd->store_globals();			/* purecov: inspected */
  else
  {
    /* Remember that we don't have a THD */
    my_pthread_setspecific_ptr(THR_THD,  0);
    my_pthread_setspecific_ptr(THR_MALLOC,  0);
  }
  DBUG_RETURN(return_val);
}


/*
  Free resources used by time zone support infrastructure.

  SYNOPSIS
    my_tz_free()
*/

void my_tz_free()
{
  if (tz_inited)
  {
    tz_inited= 0;
    VOID(pthread_mutex_destroy(&tz_LOCK));
    hash_free(&offset_tzs);
    hash_free(&tz_names);
    free_root(&tz_storage, MYF(0));
  }
}


/*
  Load time zone description from system tables.

  SYNOPSIS
    tz_load_from_open_tables()
      tz_name   - name of time zone that should be loaded.
      tz_tables - list of tables from which time zone description
                  should be loaded

  DESCRIPTION
    This function will try to load information about time zone specified
    from the list of the already opened and locked tables (first table in
    tz_tables should be time_zone_name, next time_zone, then
    time_zone_transition_type and time_zone_transition should be last).
    It will also update information in hash used for time zones lookup.

  RETURN VALUES
    Returns pointer to newly created Time_zone object or 0 in case of error.

*/

static Time_zone*
tz_load_from_open_tables(const String *tz_name, TABLE_LIST *tz_tables)
{
  TABLE *table= 0;
  TIME_ZONE_INFO *tz_info;
  Tz_names_entry *tmp_tzname;
  Time_zone *return_val= 0;
  int res;
  uint tzid, ttid;
  my_time_t ttime;
  char buff[MAX_FIELD_WIDTH];
  String abbr(buff, sizeof(buff), &my_charset_latin1);
  char *alloc_buff, *tz_name_buff;
  /*
    Temporary arrays that are used for loading of data for filling
    TIME_ZONE_INFO structure
  */
  my_time_t ats[TZ_MAX_TIMES];
  uchar types[TZ_MAX_TIMES];
  TRAN_TYPE_INFO ttis[TZ_MAX_TYPES];
#ifdef ABBR_ARE_USED
  char chars[max(TZ_MAX_CHARS + 1, (2 * (MY_TZNAME_MAX + 1)))];
#endif
  DBUG_ENTER("tz_load_from_open_tables");

  /* Prepare tz_info for loading also let us make copy of time zone name */
  if (!(alloc_buff= (char*) alloc_root(&tz_storage, sizeof(TIME_ZONE_INFO) +
                                       tz_name->length() + 1)))
  {
    sql_print_error("Out of memory while loading time zone description");
    return 0;
  }
  tz_info= (TIME_ZONE_INFO *)alloc_buff;
  bzero(tz_info, sizeof(TIME_ZONE_INFO));
  tz_name_buff= alloc_buff + sizeof(TIME_ZONE_INFO);
  /*
    By writing zero to the end we guarantee that we can call ptr()
    instead of c_ptr() for time zone name.
  */
  strmake(tz_name_buff, tz_name->ptr(), tz_name->length());

  /*
    Let us find out time zone id by its name (there is only one index
    and it is specifically for this purpose).
  */
  table= tz_tables->table;
  tz_tables= tz_tables->next_local;
  table->field[0]->store(tz_name->ptr(), tz_name->length(),
                         &my_charset_latin1);
  /*
    It is OK to ignore ha_index_init()/ha_index_end() return values since
    mysql.time_zone* tables are MyISAM and these operations always succeed
    for MyISAM.
  */
  (void)table->file->ha_index_init(0, 1);

  if (table->file->index_read_map(table->record[0], table->field[0]->ptr,
                                  HA_WHOLE_KEY, HA_READ_KEY_EXACT))
  {
#ifdef EXTRA_DEBUG
    /*
      Most probably user has mistyped time zone name, so no need to bark here
      unless we need it for debugging.
    */
    sql_print_error("Can't find description of time zone '%s'", tz_name_buff);
#endif
    goto end;
  }

  tzid= (uint)table->field[1]->val_int();

  (void)table->file->ha_index_end();

  /*
    Now we need to lookup record in mysql.time_zone table in order to
    understand whenever this timezone uses leap seconds (again we are
    using the only index in this table).
  */
  table= tz_tables->table;
  tz_tables= tz_tables->next_local;
  table->field[0]->store((longlong) tzid, TRUE);
  (void)table->file->ha_index_init(0, 1);

  if (table->file->index_read_map(table->record[0], table->field[0]->ptr,
                                  HA_WHOLE_KEY, HA_READ_KEY_EXACT))
  {
    sql_print_error("Can't find description of time zone '%u'", tzid);
    goto end;
  }

  /* If Uses_leap_seconds == 'Y' */
  if (table->field[1]->val_int() == 1)
  {
    tz_info->leapcnt= tz_leapcnt;
    tz_info->lsis= tz_lsis;
  }

  (void)table->file->ha_index_end();

  /*
    Now we will iterate through records for out time zone in
    mysql.time_zone_transition_type table. Because we want records
    only for our time zone guess what are we doing?
    Right - using special index.
  */
  table= tz_tables->table;
  tz_tables= tz_tables->next_local;
  table->field[0]->store((longlong) tzid, TRUE);
  (void)table->file->ha_index_init(0, 1);

  res= table->file->index_read_map(table->record[0], table->field[0]->ptr,
                                   (key_part_map)1, HA_READ_KEY_EXACT);
  while (!res)
  {
    ttid= (uint)table->field[1]->val_int();

    if (ttid >= TZ_MAX_TYPES)
    {
      sql_print_error("Error while loading time zone description from "
                      "mysql.time_zone_transition_type table: too big "
                      "transition type id");
      goto end;
    }

    ttis[ttid].tt_gmtoff= (long)table->field[2]->val_int();
    ttis[ttid].tt_isdst= (table->field[3]->val_int() > 0);

#ifdef ABBR_ARE_USED
    // FIXME should we do something with duplicates here ?
    table->field[4]->val_str(&abbr, &abbr);
    if (tz_info->charcnt + abbr.length() + 1 > sizeof(chars))
    {
      sql_print_error("Error while loading time zone description from "
                      "mysql.time_zone_transition_type table: not enough "
                      "room for abbreviations");
      goto end;
    }
    ttis[ttid].tt_abbrind= tz_info->charcnt;
    memcpy(chars + tz_info->charcnt, abbr.ptr(), abbr.length());
    tz_info->charcnt+= abbr.length();
    chars[tz_info->charcnt]= 0;
    tz_info->charcnt++;

    DBUG_PRINT("info",
      ("time_zone_transition_type table: tz_id=%u tt_id=%u tt_gmtoff=%ld "
       "abbr='%s' tt_isdst=%u", tzid, ttid, ttis[ttid].tt_gmtoff,
       chars + ttis[ttid].tt_abbrind, ttis[ttid].tt_isdst));
#else
    DBUG_PRINT("info",
      ("time_zone_transition_type table: tz_id=%u tt_id=%u tt_gmtoff=%ld "
       "tt_isdst=%u", tzid, ttid, ttis[ttid].tt_gmtoff, ttis[ttid].tt_isdst));
#endif

    /* ttid is increasing because we are reading using index */
    DBUG_ASSERT(ttid >= tz_info->typecnt);

    tz_info->typecnt= ttid + 1;

    res= table->file->index_next_same(table->record[0],
                                      table->field[0]->ptr, 4);
  }

  if (res != HA_ERR_END_OF_FILE)
  {
    sql_print_error("Error while loading time zone description from "
                    "mysql.time_zone_transition_type table");
    goto end;
  }

  (void)table->file->ha_index_end();


  /*
    At last we are doing the same thing for records in
    mysql.time_zone_transition table. Here we additionaly need records
    in ascending order by index scan also satisfies us.
  */
  table= tz_tables->table; 
  table->field[0]->store((longlong) tzid, TRUE);
  (void)table->file->ha_index_init(0, 1);

  res= table->file->index_read_map(table->record[0], table->field[0]->ptr,
                                   (key_part_map)1, HA_READ_KEY_EXACT);
  while (!res)
  {
    ttime= (my_time_t)table->field[1]->val_int();
    ttid= (uint)table->field[2]->val_int();

    if (tz_info->timecnt + 1 > TZ_MAX_TIMES)
    {
      sql_print_error("Error while loading time zone description from "
                      "mysql.time_zone_transition table: "
                      "too much transitions");
      goto end;
    }
    if (ttid + 1 > tz_info->typecnt)
    {
      sql_print_error("Error while loading time zone description from "
                      "mysql.time_zone_transition table: "
                      "bad transition type id");
      goto end;
    }

    ats[tz_info->timecnt]= ttime;
    types[tz_info->timecnt]= ttid;
    tz_info->timecnt++;

    DBUG_PRINT("info",
      ("time_zone_transition table: tz_id: %u  tt_time: %lu  tt_id: %u",
       tzid, (ulong) ttime, ttid));

    res= table->file->index_next_same(table->record[0],
                                      table->field[0]->ptr, 4);
  }

  /*
    We have to allow HA_ERR_KEY_NOT_FOUND because some time zones
    for example UTC have no transitons.
  */
  if (res != HA_ERR_END_OF_FILE && res != HA_ERR_KEY_NOT_FOUND)
  {
    sql_print_error("Error while loading time zone description from "
                    "mysql.time_zone_transition table");
    goto end;
  }

  (void)table->file->ha_index_end();
  table= 0;

  /*
    Now we will allocate memory and init TIME_ZONE_INFO structure.
  */
  if (!(alloc_buff= (char*) alloc_root(&tz_storage,
                                       ALIGN_SIZE(sizeof(my_time_t) *
                                                  tz_info->timecnt) +
                                       ALIGN_SIZE(tz_info->timecnt) +
#ifdef ABBR_ARE_USED
                                       ALIGN_SIZE(tz_info->charcnt) +
#endif
                                       sizeof(TRAN_TYPE_INFO) *
                                       tz_info->typecnt)))
  {
    sql_print_error("Out of memory while loading time zone description");
    goto end;
  }

  tz_info->ats= (my_time_t *) alloc_buff;
  memcpy(tz_info->ats, ats, tz_info->timecnt * sizeof(my_time_t));
  alloc_buff+= ALIGN_SIZE(sizeof(my_time_t) * tz_info->timecnt);
  tz_info->types= (uchar *)alloc_buff;
  memcpy(tz_info->types, types, tz_info->timecnt);
  alloc_buff+= ALIGN_SIZE(tz_info->timecnt);
#ifdef ABBR_ARE_USED
  tz_info->chars= alloc_buff;
  memcpy(tz_info->chars, chars, tz_info->charcnt);
  alloc_buff+= ALIGN_SIZE(tz_info->charcnt);
#endif
  tz_info->ttis= (TRAN_TYPE_INFO *)alloc_buff;
  memcpy(tz_info->ttis, ttis, tz_info->typecnt * sizeof(TRAN_TYPE_INFO));

  /*
    Let us check how correct our time zone description and build
    reversed map. We don't check for tz->timecnt < 1 since it ok for GMT.
  */
  if (tz_info->typecnt < 1)
  {
    sql_print_error("loading time zone without transition types");
    goto end;
  }
  if (prepare_tz_info(tz_info, &tz_storage))
  {
    sql_print_error("Unable to build mktime map for time zone");
    goto end;
  }


  if (!(tmp_tzname= new (&tz_storage) Tz_names_entry()) ||
      !(tmp_tzname->tz= new (&tz_storage) Time_zone_db(tz_info,
                                            &(tmp_tzname->name))) ||
      (tmp_tzname->name.set(tz_name_buff, tz_name->length(),
                            &my_charset_latin1),
       my_hash_insert(&tz_names, (const uchar *)tmp_tzname)))
  {
    sql_print_error("Out of memory while loading time zone");
    goto end;
  }

  /*
    Loading of time zone succeeded
  */
  return_val= tmp_tzname->tz;

end:

  if (table)
    (void)table->file->ha_index_end();

  DBUG_RETURN(return_val);
}


/*
  Parse string that specifies time zone as offset from UTC.

  SYNOPSIS
    str_to_offset()
      str    - pointer to string which contains offset
      length - length of string
      offset - out parameter for storing found offset in seconds.

  DESCRIPTION
    This function parses string which contains time zone offset
    in form similar to '+10:00' and converts found value to
    seconds from UTC form (east is positive).

  RETURN VALUE
    0 - Ok
    1 - String doesn't contain valid time zone offset
*/
my_bool
str_to_offset(const char *str, uint length, long *offset)
{
  const char *end= str + length;
  my_bool negative;
  ulong number_tmp;
  long offset_tmp;

  if (length < 4)
    return 1;

  if (*str == '+')
    negative= 0;
  else if (*str == '-')
    negative= 1;
  else
    return 1;
  str++;

  number_tmp= 0;

  while (str < end && my_isdigit(&my_charset_latin1, *str))
  {
    number_tmp= number_tmp*10 + *str - '0';
    str++;
  }

  if (str + 1 >= end || *str != ':')
    return 1;
  str++;

  offset_tmp = number_tmp * MINS_PER_HOUR; number_tmp= 0;

  while (str < end && my_isdigit(&my_charset_latin1, *str))
  {
    number_tmp= number_tmp * 10 + *str - '0';
    str++;
  }

  if (str != end)
    return 1;

  offset_tmp= (offset_tmp + number_tmp) * SECS_PER_MIN;

  if (negative)
    offset_tmp= -offset_tmp;

  /*
    Check if offset is in range prescribed by standard
    (from -12:59 to 13:00).
  */

  if (number_tmp > 59 || offset_tmp < -13 * SECS_PER_HOUR + 1 ||
      offset_tmp > 13 * SECS_PER_HOUR)
    return 1;

  *offset= offset_tmp;

  return 0;
}


/*
  Get Time_zone object for specified time zone.

  SYNOPSIS
    my_tz_find()
      thd  - pointer to thread THD structure
      name - time zone specification

  DESCRIPTION
    This function checks if name is one of time zones described in db,
    predefined SYSTEM time zone or valid time zone specification as
    offset from UTC (In last case it will create proper Time_zone_offset
    object if there were not any.). If name is ok it returns corresponding
    Time_zone object.

    Clients of this function are not responsible for releasing resources
    occupied by returned Time_zone object so they can just forget pointers
    to Time_zone object if they are not needed longer.

    Other important property of this function: if some Time_zone found once
    it will be for sure found later, so this function can also be used for
    checking if proper Time_zone object exists (and if there will be error
    it will be reported during first call).

    If name pointer is 0 then this function returns 0 (this allows to pass 0
    values as parameter without additional external check and this property
    is used by @@time_zone variable handling code).

    It will perform lookup in system tables (mysql.time_zone*),
    opening and locking them, and closing afterwards. It won't perform
    such lookup if no time zone describing tables were found during
    server start up.

  RETURN VALUE
    Pointer to corresponding Time_zone object. 0 - in case of bad time zone
    specification or other error.

*/
Time_zone *
my_tz_find(THD *thd, const String *name)
{
  Tz_names_entry *tmp_tzname;
  Time_zone *result_tz= 0;
  long offset;
  DBUG_ENTER("my_tz_find");
  DBUG_PRINT("enter", ("time zone name='%s'",
                       name ? ((String *)name)->c_ptr_safe() : "NULL"));

  if (!name)
    DBUG_RETURN(0);

  VOID(pthread_mutex_lock(&tz_LOCK));

  if (!str_to_offset(name->ptr(), name->length(), &offset))
  {

    if (!(result_tz= (Time_zone_offset *)hash_search(&offset_tzs,
                                                     (const uchar *)&offset,
                                                     sizeof(long))))
    {
      DBUG_PRINT("info", ("Creating new Time_zone_offset object"));

      if (!(result_tz= new (&tz_storage) Time_zone_offset(offset)) ||
          my_hash_insert(&offset_tzs, (const uchar *) result_tz))
      {
        result_tz= 0;
        sql_print_error("Fatal error: Out of memory "
                        "while setting new time zone");
      }
    }
  }
  else
  {
    result_tz= 0;
    if ((tmp_tzname= (Tz_names_entry *)hash_search(&tz_names,
                                                   (const uchar *)name->ptr(),
                                                   name->length())))
      result_tz= tmp_tzname->tz;
    else if (time_zone_tables_exist)
    {
      TABLE_LIST tz_tables[MY_TZ_TABLES_COUNT];
      Open_tables_state open_tables_state_backup;

      tz_init_table_list(tz_tables);
      if (!open_system_tables_for_read(thd, tz_tables,
                                       &open_tables_state_backup))
      {
        result_tz= tz_load_from_open_tables(name, tz_tables);
        close_system_tables(thd, &open_tables_state_backup);
      }
    }
  }

  VOID(pthread_mutex_unlock(&tz_LOCK));

  DBUG_RETURN(result_tz);
}


#endif /* !defined(TESTTIME) && !defined(TZINFO2SQL) */


#ifdef TZINFO2SQL
/*
  This code belongs to mysql_tzinfo_to_sql converter command line utility.
  This utility should be used by db admin for populating mysql.time_zone
  tables.
*/


/*
  Print info about time zone described by TIME_ZONE_INFO struct as
  SQL statements populating mysql.time_zone* tables.

  SYNOPSIS
    print_tz_as_sql()
      tz_name - name of time zone
      sp      - structure describing time zone
*/
void
print_tz_as_sql(const char* tz_name, const TIME_ZONE_INFO *sp)
{
  uint i;

  /* Here we assume that all time zones have same leap correction tables */
  printf("INSERT INTO time_zone (Use_leap_seconds) VALUES ('%s');\n",
         sp->leapcnt ? "Y" : "N");
  printf("SET @time_zone_id= LAST_INSERT_ID();\n");
  printf("INSERT INTO time_zone_name (Name, Time_zone_id) VALUES \
('%s', @time_zone_id);\n", tz_name);

  if (sp->timecnt)
  {
    printf("INSERT INTO time_zone_transition \
(Time_zone_id, Transition_time, Transition_type_id) VALUES\n");
    for (i= 0; i < sp->timecnt; i++)
      printf("%s(@time_zone_id, %ld, %u)\n", (i == 0 ? " " : ","), sp->ats[i],
             (uint)sp->types[i]);
    printf(";\n");
  }

  printf("INSERT INTO time_zone_transition_type \
(Time_zone_id, Transition_type_id, Offset, Is_DST, Abbreviation) VALUES\n");

  for (i= 0; i < sp->typecnt; i++)
    printf("%s(@time_zone_id, %u, %ld, %d, '%s')\n", (i == 0 ? " " : ","), i,
           sp->ttis[i].tt_gmtoff, sp->ttis[i].tt_isdst,
           sp->chars + sp->ttis[i].tt_abbrind);
  printf(";\n");
}


/*
  Print info about leap seconds in time zone as SQL statements
  populating mysql.time_zone_leap_second table.

  SYNOPSIS
    print_tz_leaps_as_sql()
      sp      - structure describing time zone
*/
void
print_tz_leaps_as_sql(const TIME_ZONE_INFO *sp)
{
  uint i;

  /*
    We are assuming that there are only one list of leap seconds
    For all timezones.
  */
  printf("TRUNCATE TABLE time_zone_leap_second;\n");

  if (sp->leapcnt)
  {
    printf("INSERT INTO time_zone_leap_second \
(Transition_time, Correction) VALUES\n");
    for (i= 0; i < sp->leapcnt; i++)
      printf("%s(%ld, %ld)\n", (i == 0 ? " " : ","),
             sp->lsis[i].ls_trans, sp->lsis[i].ls_corr);
    printf(";\n");
  }

  printf("ALTER TABLE time_zone_leap_second ORDER BY Transition_time;\n");
}


/*
  Some variables used as temporary or as parameters
  in recursive scan_tz_dir() code.
*/
TIME_ZONE_INFO tz_info;
MEM_ROOT tz_storage;
char fullname[FN_REFLEN + 1];
char *root_name_end;


/*
  Recursively scan zoneinfo directory and print all found time zone
  descriptions as SQL.

  SYNOPSIS
    scan_tz_dir()
      name_end - pointer to end of path to directory to be searched.

  DESCRIPTION
    This auxiliary recursive function also uses several global
    variables as in parameters and for storing temporary values.

    fullname      - path to directory that should be scanned.
    root_name_end - pointer to place in fullname where part with
                    path to initial directory ends.
    current_tz_id - last used time zone id

  RETURN VALUE
    0 - Ok, 1 - Fatal error

*/
my_bool
scan_tz_dir(char * name_end)
{
  MY_DIR *cur_dir;
  char *name_end_tmp;
  uint i;

  if (!(cur_dir= my_dir(fullname, MYF(MY_WANT_STAT))))
    return 1;

  name_end= strmake(name_end, "/", FN_REFLEN - (name_end - fullname));

  for (i= 0; i < cur_dir->number_off_files; i++)
  {
    if (cur_dir->dir_entry[i].name[0] != '.')
    {
      name_end_tmp= strmake(name_end, cur_dir->dir_entry[i].name,
                            FN_REFLEN - (name_end - fullname));

      if (MY_S_ISDIR(cur_dir->dir_entry[i].mystat->st_mode))
      {
        if (scan_tz_dir(name_end_tmp))
        {
          my_dirend(cur_dir);
          return 1;
        }
      }
      else if (MY_S_ISREG(cur_dir->dir_entry[i].mystat->st_mode))
      {
        init_alloc_root(&tz_storage, 32768, 0);
        if (!tz_load(fullname, &tz_info, &tz_storage))
          print_tz_as_sql(root_name_end + 1, &tz_info);
        else
          fprintf(stderr,
                  "Warning: Unable to load '%s' as time zone. Skipping it.\n",
                  fullname);
        free_root(&tz_storage, MYF(0));
      }
      else
        fprintf(stderr, "Warning: '%s' is not regular file or directory\n",
                fullname);
    }
  }

  my_dirend(cur_dir);

  return 0;
}


int
main(int argc, char **argv)
{
#ifndef __NETWARE__
  MY_INIT(argv[0]);

  if (argc != 2 && argc != 3)
  {
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, " %s timezonedir\n", argv[0]);
    fprintf(stderr, " %s timezonefile timezonename\n", argv[0]);
    fprintf(stderr, " %s --leap timezonefile\n", argv[0]);
    return 1;
  }

  if (argc == 2)
  {
    root_name_end= strmake(fullname, argv[1], FN_REFLEN);

    printf("TRUNCATE TABLE time_zone;\n");
    printf("TRUNCATE TABLE time_zone_name;\n");
    printf("TRUNCATE TABLE time_zone_transition;\n");
    printf("TRUNCATE TABLE time_zone_transition_type;\n");

    if (scan_tz_dir(root_name_end))
    {
      fprintf(stderr, "There were fatal errors during processing "
                      "of zoneinfo directory\n");
      return 1;
    }

    printf("ALTER TABLE time_zone_transition "
           "ORDER BY Time_zone_id, Transition_time;\n");
    printf("ALTER TABLE time_zone_transition_type "
           "ORDER BY Time_zone_id, Transition_type_id;\n");
  }
  else
  {
    init_alloc_root(&tz_storage, 32768, 0);

    if (strcmp(argv[1], "--leap") == 0)
    {
      if (tz_load(argv[2], &tz_info, &tz_storage))
      {
        fprintf(stderr, "Problems with zoneinfo file '%s'\n", argv[2]);
        return 1;
      }
      print_tz_leaps_as_sql(&tz_info);
    }
    else
    {
      if (tz_load(argv[1], &tz_info, &tz_storage))
      {
        fprintf(stderr, "Problems with zoneinfo file '%s'\n", argv[2]);
        return 1;
      }
      print_tz_as_sql(argv[2], &tz_info);
    }

    free_root(&tz_storage, MYF(0));
  }

#else
  fprintf(stderr, "This tool has not been ported to NetWare\n");
#endif /* __NETWARE__ */

  return 0;
}

#endif /* defined(TZINFO2SQL) */


#ifdef TESTTIME

/*
   Some simple brute-force test wich allowed to catch a pair of bugs.
   Also can provide interesting facts about system's time zone support
   implementation.
*/

#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif

#ifndef TYPE_BIT
#define TYPE_BIT(type)	(sizeof (type) * CHAR_BIT)
#endif

#ifndef TYPE_SIGNED
#define TYPE_SIGNED(type) (((type) -1) < 0)
#endif

my_bool
is_equal_TIME_tm(const TIME* time_arg, const struct tm * tm_arg)
{
  return (time_arg->year == (uint)tm_arg->tm_year+TM_YEAR_BASE) &&
         (time_arg->month == (uint)tm_arg->tm_mon+1) &&
         (time_arg->day == (uint)tm_arg->tm_mday) &&
         (time_arg->hour == (uint)tm_arg->tm_hour) &&
         (time_arg->minute == (uint)tm_arg->tm_min) &&
         (time_arg->second == (uint)tm_arg->tm_sec) &&
         time_arg->second_part == 0;
}


int
main(int argc, char **argv)
{
  my_bool localtime_negative;
  TIME_ZONE_INFO tz_info;
  struct tm tmp;
  MYSQL_TIME time_tmp;
  time_t t, t1, t2;
  char fullname[FN_REFLEN+1];
  char *str_end;
  MEM_ROOT tz_storage;

  MY_INIT(argv[0]);

  init_alloc_root(&tz_storage, 32768, 0);

  /* let us set some well known timezone */
  setenv("TZ", "MET", 1);
  tzset();

  /* Some initial time zone related system info */
  printf("time_t: %s %u bit\n", TYPE_SIGNED(time_t) ? "signed" : "unsigned",
                                (uint)TYPE_BIT(time_t));
  if (TYPE_SIGNED(time_t))
  {
    t= -100;
    localtime_negative= test(localtime_r(&t, &tmp) != 0);
    printf("localtime_r %s negative params \
           (time_t=%d is %d-%d-%d %d:%d:%d)\n",
           (localtime_negative ? "supports" : "doesn't support"), (int)t,
           TM_YEAR_BASE + tmp.tm_year, tmp.tm_mon + 1, tmp.tm_mday,
           tmp.tm_hour, tmp.tm_min, tmp.tm_sec);

    printf("mktime %s negative results (%d)\n",
           (t == mktime(&tmp) ? "doesn't support" : "supports"),
           (int)mktime(&tmp));
  }

  tmp.tm_year= 103; tmp.tm_mon= 2; tmp.tm_mday= 30;
  tmp.tm_hour= 2; tmp.tm_min= 30; tmp.tm_sec= 0; tmp.tm_isdst= -1;
  t= mktime(&tmp);
  printf("mktime returns %s for spring time gap (%d)\n",
         (t != (time_t)-1 ? "something" : "error"), (int)t);

  tmp.tm_year= 103; tmp.tm_mon= 8; tmp.tm_mday= 1;
  tmp.tm_hour= 0; tmp.tm_min= 0; tmp.tm_sec= 0; tmp.tm_isdst= 0;
  t= mktime(&tmp);
  printf("mktime returns %s for non existing date (%d)\n",
         (t != (time_t)-1 ? "something" : "error"), (int)t);

  tmp.tm_year= 103; tmp.tm_mon= 8; tmp.tm_mday= 1;
  tmp.tm_hour= 25; tmp.tm_min=0; tmp.tm_sec=0; tmp.tm_isdst=1;
  t= mktime(&tmp);
  printf("mktime %s unnormalized input (%d)\n",
         (t != (time_t)-1 ? "handles" : "doesn't handle"), (int)t);

  tmp.tm_year= 103; tmp.tm_mon= 9; tmp.tm_mday= 26;
  tmp.tm_hour= 0; tmp.tm_min= 30; tmp.tm_sec= 0; tmp.tm_isdst= 1;
  mktime(&tmp);
  tmp.tm_hour= 2; tmp.tm_isdst= -1;
  t= mktime(&tmp);
  tmp.tm_hour= 4; tmp.tm_isdst= 0;
  mktime(&tmp);
  tmp.tm_hour= 2; tmp.tm_isdst= -1;
  t1= mktime(&tmp);
  printf("mktime is %s (%d %d)\n",
         (t == t1 ? "determenistic" : "is non-determenistic"),
         (int)t, (int)t1);

  /* Let us load time zone description */
  str_end= strmake(fullname, TZDIR, FN_REFLEN);
  strmake(str_end, "/MET", FN_REFLEN - (str_end - fullname));

  if (tz_load(fullname, &tz_info, &tz_storage))
  {
    printf("Unable to load time zone info from '%s'\n", fullname);
    free_root(&tz_storage, MYF(0));
    return 1;
  }

  printf("Testing our implementation\n");

  if (TYPE_SIGNED(time_t) && localtime_negative)
  {
    for (t= -40000; t < 20000; t++)
    {
      localtime_r(&t, &tmp);
      gmt_sec_to_TIME(&time_tmp, (my_time_t)t, &tz_info);
      if (!is_equal_TIME_tm(&time_tmp, &tmp))
      {
        printf("Problem with negative time_t = %d\n", (int)t);
        free_root(&tz_storage, MYF(0));
        return 1;
      }
    }
    printf("gmt_sec_to_TIME = localtime for time_t in [-40000,20000) range\n");
  }

  for (t= 1000000000; t < 1100000000; t+= 13)
  {
    localtime_r(&t,&tmp);
    gmt_sec_to_TIME(&time_tmp, (my_time_t)t, &tz_info);

    if (!is_equal_TIME_tm(&time_tmp, &tmp))
    {
      printf("Problem with time_t = %d\n", (int)t);
      free_root(&tz_storage, MYF(0));
      return 1;
    }
  }
  printf("gmt_sec_to_TIME = localtime for time_t in [1000000000,1100000000) range\n");

  init_time();

  /*
    Be careful here! my_system_gmt_sec doesn't fully handle unnormalized
    dates.
  */
  for (time_tmp.year= 1980; time_tmp.year < 2010; time_tmp.year++)
  {
    for (time_tmp.month= 1; time_tmp.month < 13; time_tmp.month++)
    {
      for (time_tmp.day= 1;
           time_tmp.day < mon_lengths[isleap(time_tmp.year)][time_tmp.month-1];
           time_tmp.day++)
      {
        for (time_tmp.hour= 0; time_tmp.hour < 24; time_tmp.hour++)
        {
          for (time_tmp.minute= 0; time_tmp.minute < 60; time_tmp.minute+= 5)
          {
            for (time_tmp.second=0; time_tmp.second<60; time_tmp.second+=25)
            {
              long not_used;
              my_bool not_used_2;
              t= (time_t)my_system_gmt_sec(&time_tmp, &not_used, &not_used_2);
              t1= (time_t)TIME_to_gmt_sec(&time_tmp, &tz_info, &not_used_2);
              if (t != t1)
              {
                /*
                  We need special handling during autumn since my_system_gmt_sec
                  prefers greater time_t values (in MET) for ambiguity.
                  And BTW that is a bug which should be fixed !!!
                */
                tmp.tm_year= time_tmp.year - TM_YEAR_BASE;
                tmp.tm_mon= time_tmp.month - 1;
                tmp.tm_mday= time_tmp.day;
                tmp.tm_hour= time_tmp.hour;
                tmp.tm_min= time_tmp.minute;
                tmp.tm_sec= time_tmp.second;
                tmp.tm_isdst= 1;

                t2= mktime(&tmp);

                if (t1 == t2)
                  continue;

                printf("Problem: %u/%u/%u %u:%u:%u with times t=%d, t1=%d\n",
                       time_tmp.year, time_tmp.month, time_tmp.day,
                       time_tmp.hour, time_tmp.minute, time_tmp.second,
                       (int)t,(int)t1);

                free_root(&tz_storage, MYF(0));
                return 1;
              }
            }
          }
        }
      }
    }
  }

  printf("TIME_to_gmt_sec = my_system_gmt_sec for test range\n");

  free_root(&tz_storage, MYF(0));
  return 0;
}

#endif /* defined(TESTTIME) */
