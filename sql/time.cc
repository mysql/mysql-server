/* Copyright (C) 2000 MySQL AB & MySQL Finland AB & TCX DataKonsult AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


/* Functions to handle date and time */

#include "mysql_priv.h"
#include <m_ctype.h>

static ulong const days_at_timestart=719528;	/* daynr at 1970.01.01 */
uchar *days_in_month= (uchar*) "\037\034\037\036\037\036\037\037\036\037\036\037";


	/* Init some variabels needed when using my_local_time */
	/* Currently only my_time_zone is inited */

static long my_time_zone=0;

void init_time(void)
{
  time_t seconds;
  struct tm *l_time,tm_tmp;;
  TIME my_time;

  seconds= (time_t) time((time_t*) 0);
  localtime_r(&seconds,&tm_tmp);
  l_time= &tm_tmp;
  my_time_zone=		3600;		/* Comp. for -3600 in my_gmt_sec */
  my_time.year=		(uint) l_time->tm_year+1900;
  my_time.month=	(uint) l_time->tm_mon+1;
  my_time.day=		(uint) l_time->tm_mday;
  my_time.hour=		(uint) l_time->tm_hour;
  my_time.minute=	(uint) l_time->tm_min;
  my_time.second=	(uint) l_time->tm_sec;
  my_gmt_sec(&my_time, &my_time_zone);	/* Init my_time_zone */
}

/*
  Convert current time to sec. since 1970.01.01 
  This code handles also day light saving time.
  The idea is to cache the time zone (including daylight saving time)
  for the next call to make things faster.

*/

long my_gmt_sec(TIME *t, long *my_timezone)
{
  uint loop;
  time_t tmp;
  struct tm *l_time,tm_tmp;
  long diff, current_timezone;

  if (t->year > TIMESTAMP_MAX_YEAR || t->year < TIMESTAMP_MIN_YEAR)
    return 0;
    
  if (t->hour >= 24)
  {					/* Fix for time-loop */
    t->day+=t->hour/24;
    t->hour%=24;
  }

  /*
    Calculate the gmt time based on current time and timezone
    The -1 on the end is to ensure that if have a date that exists twice
    (like 2002-10-27 02:00:0 MET), we will find the initial date.

    By doing -3600 we will have to call localtime_r() several times, but
    I couldn't come up with a better way to get a repeatable result :(

    We can't use mktime() as it's buggy on many platforms and not thread safe.

    Note: this code assumes that our time_t estimation is not too far away
    from real value (we assume that localtime_r(tmp) will return something
    within 24 hrs from t) which is probably true for all current time zones.
  */
  tmp=(time_t) (((calc_daynr((uint) t->year,(uint) t->month,(uint) t->day) -
		  (long) days_at_timestart)*86400L + (long) t->hour*3600L +
		 (long) (t->minute*60 + t->second)) + (time_t) my_time_zone -
		3600);
  current_timezone= my_time_zone;

  localtime_r(&tmp,&tm_tmp);
  l_time=&tm_tmp;
  for (loop=0;
       loop < 2 &&
	 (t->hour != (uint) l_time->tm_hour ||
          t->minute != (uint) l_time->tm_min ||
          t->second != (uint) l_time->tm_sec);
       loop++)
  {					/* One check should be enough ? */
    /* Get difference in days */
    int days= t->day - l_time->tm_mday;
    if (days < -1)
      days= 1;					// Month has wrapped
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour)) +
          (long) (60*((int) t->minute - (int) l_time->tm_min)) +
          (long) ((int) t->second - (int) l_time->tm_sec));
    current_timezone+= diff+3600;		// Compensate for -3600 above
    tmp+= (time_t) diff;
    localtime_r(&tmp,&tm_tmp);
    l_time=&tm_tmp;
  }
  /*
    Fix that if we are in the non existing daylight saving time hour
    we move the start of the next real hour.

    This code doesn't handle such exotical thing as time-gaps whose length
    is more than one hour or non-integer (latter can theoretically happen
    if one of seconds will be removed due leap correction, or because of
    general time correction like it happened for Africa/Monrovia time zone
    in year 1972).
  */
  if (loop == 2 && t->hour != (uint) l_time->tm_hour)
  {
    int days= t->day - l_time->tm_mday;
    if (days < -1)
      days=1;					// Month has wrapped
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour))+
	  (long) (60*((int) t->minute - (int) l_time->tm_min)) +
          (long) ((int) t->second - (int) l_time->tm_sec));
    if (diff == 3600)
      tmp+=3600 - t->minute*60 - t->second;	// Move to next hour
    else if (diff == -3600)
      tmp-=t->minute*60 + t->second;		// Move to previous hour
  }
  *my_timezone= current_timezone;
  
  if (tmp < TIMESTAMP_MIN_VALUE || tmp > TIMESTAMP_MAX_VALUE)
    tmp= 0;
  
  return (long) tmp;
} /* my_gmt_sec */


	/* Some functions to calculate dates */

	/* Calculate nr of day since year 0 in new date-system (from 1615) */

long calc_daynr(uint year,uint month,uint day)
{
  long delsum;
  int temp;
  DBUG_ENTER("calc_daynr");

  if (year == 0 && month == 0 && day == 0)
    DBUG_RETURN(0);				/* Skip errors */
  if (year < 200)
  {
    if ((year=year+1900) < 1900+YY_PART_YEAR)
      year+=100;
  }
  delsum= (long) (365L * year+ 31*(month-1) +day);
  if (month <= 2)
      year--;
  else
    delsum-= (long) (month*4+23)/10;
  temp=(int) ((year/100+1)*3)/4;
  DBUG_PRINT("exit",("year: %d  month: %d  day: %d -> daynr: %ld",
		     year+(month <= 2),month,day,delsum+year/4-temp));
  DBUG_RETURN(delsum+(int) year/4-temp);
} /* calc_daynr */


	/* Calc weekday from daynr */
	/* Returns 0 for monday, 1 for tuesday .... */

int calc_weekday(long daynr,bool sunday_first_day_of_week)
{
  DBUG_ENTER("calc_weekday");
  DBUG_RETURN ((int) ((daynr + 5L + (sunday_first_day_of_week ? 1L : 0L)) % 7));
}

	/* Calc days in one year. works with 0 <= year <= 99 */

uint calc_days_in_year(uint year)
{
  return (year & 3) == 0 && (year%100 || (year%400 == 0 && year)) ?
    366 : 365;
}


/*
  The bits in week_format has the following meaning:
   WEEK_MONDAY_FIRST (0)  If not set	Sunday is first day of week
      		   	  If set	Monday is first day of week
   WEEK_YEAR (1)	  If not set	Week is in range 0-53

   	Week 0 is returned for the the last week of the previous year (for
	a date at start of january) In this case one can get 53 for the
	first week of next year.  This flag ensures that the week is
	relevant for the given year. Note that this flag is only
	releveant if WEEK_JANUARY is not set.

			  If set	 Week is in range 1-53.

	In this case one may get week 53 for a date in January (when
	the week is that last week of previous year) and week 1 for a
	date in December.

  WEEK_FIRST_WEEKDAY (2)  If not set	Weeks are numbered according
			   		to ISO 8601:1988
			  If set	The week that contains the first
					'first-day-of-week' is week 1.
	
	ISO 8601:1988 means that if the week containing January 1 has
	four or more days in the new year, then it is week 1;
	Otherwise it is the last week of the previous year, and the
	next week is week 1.
*/

uint calc_week(TIME *l_time, uint week_behaviour, uint *year)
{
  uint days;
  ulong daynr=calc_daynr(l_time->year,l_time->month,l_time->day);
  ulong first_daynr=calc_daynr(l_time->year,1,1);
  bool monday_first= test(week_behaviour & WEEK_MONDAY_FIRST);
  bool week_year= test(week_behaviour & WEEK_YEAR);
  bool first_weekday= test(week_behaviour & WEEK_FIRST_WEEKDAY);

  uint weekday=calc_weekday(first_daynr, !monday_first);
  *year=l_time->year;

  if (l_time->month == 1 && l_time->day <= 7-weekday)
  {
    if (!week_year && 
	(first_weekday && weekday != 0 ||
	 !first_weekday && weekday >= 4))
      return 0;
    week_year= 1;
    (*year)--;
    first_daynr-= (days=calc_days_in_year(*year));
    weekday= (weekday + 53*7- days) % 7;
  }

  if ((first_weekday && weekday != 0) ||
      (!first_weekday && weekday >= 4))
    days= daynr - (first_daynr+ (7-weekday));
  else
    days= daynr - (first_daynr - weekday);

  if (week_year && days >= 52*7)
  {
    weekday= (weekday + calc_days_in_year(*year)) % 7;
    if (!first_weekday && weekday < 4 ||
	first_weekday && weekday == 0)
    {
      (*year)++;
      return 1;
    }
  }
  return days/7+1;
}

	/* Change a daynr to year, month and day */
	/* Daynr 0 is returned as date 00.00.00 */

void get_date_from_daynr(long daynr,uint *ret_year,uint *ret_month,
			 uint *ret_day)
{
  uint year,temp,leap_day,day_of_year,days_in_year;
  uchar *month_pos;
  DBUG_ENTER("get_date_from_daynr");

  if (daynr <= 365L || daynr >= 3652500)
  {						/* Fix if wrong daynr */
    *ret_year= *ret_month = *ret_day =0;
  }
  else
  {
    year= (uint) (daynr*100 / 36525L);
    temp=(((year-1)/100+1)*3)/4;
    day_of_year=(uint) (daynr - (long) year * 365L) - (year-1)/4 +temp;
    while (day_of_year > (days_in_year= calc_days_in_year(year)))
    {
      day_of_year-=days_in_year;
      (year)++;
    }
    leap_day=0;
    if (days_in_year == 366)
    {
      if (day_of_year > 31+28)
      {
	day_of_year--;
	if (day_of_year == 31+28)
	  leap_day=1;		/* Handle leapyears leapday */
      }
    }
    *ret_month=1;
    for (month_pos= days_in_month ;
	 day_of_year > (uint) *month_pos ;
	 day_of_year-= *(month_pos++), (*ret_month)++)
      ;
    *ret_year=year;
    *ret_day=day_of_year+leap_day;
  }
  DBUG_VOID_RETURN;
}

	/* Functions to handle periods */

ulong convert_period_to_month(ulong period)
{
  ulong a,b;
  if (period == 0)
    return 0L;
  if ((a=period/100) < YY_PART_YEAR)
    a+=2000;
  else if (a < 100)
    a+=1900;
  b=period%100;
  return a*12+b-1;
}

ulong convert_month_to_period(ulong month)
{
  ulong year;
  if (month == 0L)
    return 0L;
  if ((year=month/12) < 100)
  {
    year+=(year < YY_PART_YEAR) ? 2000 : 1900;
  }
  return year*100+month%12+1;
}


/*****************************************************************************
** convert a timestamp string to a TIME value.
** At least the following formats are recogniced (based on number of digits)
** YYMMDD, YYYYMMDD, YYMMDDHHMMSS, YYYYMMDDHHMMSS
** YY-MM-DD, YYYY-MM-DD, YY-MM-DD HH.MM.SS
** Returns the type of string
*****************************************************************************/

timestamp_type
str_to_TIME(const char *str, uint length, TIME *l_time,bool fuzzy_date)
{
  uint field_length,year_length,digits,i,number_of_fields,date[7];
  uint not_zero_date;
  const char *pos;
  const char *end=str+length;
  DBUG_ENTER("str_to_TIME");
  DBUG_PRINT("enter",("str: %.*s",length,str));

  for (; str != end && !isdigit(*str) ; str++) ; // Skip garbage
  if (str == end)
    DBUG_RETURN(TIMESTAMP_NONE);
  /*
  ** calculate first number of digits.
  ** If length= 8 or >= 14 then year is of format YYYY.
     (YYYY-MM-DD,  YYYYMMDD, YYYYYMMDDHHMMSS)
  */
  for (pos=str; pos != end && isdigit(*pos) ; pos++) ;
  digits= (uint) (pos-str);
  year_length= (digits == 4 || digits == 8 || digits >= 14) ? 4 : 2;
  field_length=year_length-1;
  not_zero_date= 0;
  for (i=0 ; i < 6 && str != end && isdigit(*str) ; i++)
  {
    uint tmp_value=(uint) (uchar) (*str++ - '0');
    while (str != end && isdigit(str[0]) && field_length--)
    {
      tmp_value=tmp_value*10 + (uint) (uchar) (*str - '0');
      str++;
    }
    date[i]=tmp_value;
    not_zero_date|= tmp_value;
    if (i == 2 && str != end && *str == 'T')
      str++;					// ISO8601:  CCYYMMDDThhmmss
    else if ( i != 5 ) 				// Skip inter-field delimiters 
    {
      while (str != end && (ispunct(*str) || isspace(*str)))
      {
	// Only allow space between days and hours
	if (isspace(*str) && i != 2)
	  DBUG_RETURN(TIMESTAMP_NONE);
	str++;
      }
    }
    field_length=1;				// Rest fields can only be 2
  }
  /* Handle second fractions */
  if (i == 6 && (uint) (end-str) >= 2 && *str == '.' && isdigit(str[1]))
  {
    str++;
    uint tmp_value=(uint) (uchar) (*str - '0');
    field_length=3;
    while (str++ != end && isdigit(str[0]) && field_length--)
      tmp_value=tmp_value*10 + (uint) (uchar) (*str - '0');
    date[6]=tmp_value;
    not_zero_date|= tmp_value;
  }
  else
    date[6]=0;

  if (year_length == 2 && i >=2 && (date[1] || date[2]))
    date[0]+= (date[0] < YY_PART_YEAR ? 2000 : 1900);
  number_of_fields=i;
  while (i < 6)
    date[i++]=0;
  if (number_of_fields < 3 || date[1] > 12 ||
      date[2] > 31 || date[3] > 23 || date[4] > 59 || date[5] > 59 ||
      (!fuzzy_date && (date[1] == 0 || date[2] == 0)))
  {
    /* Only give warning for a zero date if there is some garbage after */
    if (!not_zero_date)				// If zero date
    {
      for (; str != end ; str++)
      {
	if (!isspace(*str))
	{
	  not_zero_date= 1;			// Give warning
	  break;
	}
      }
    }
    if (not_zero_date)
      current_thd->cuted_fields++;
    DBUG_RETURN(TIMESTAMP_NONE);
  }
  if (str != end && current_thd->count_cuted_fields)
  {
    for (; str != end ; str++)
    {
      if (!isspace(*str))
      {
	current_thd->cuted_fields++;
	break;
      }
    }
  }
  l_time->year=  date[0];
  l_time->month= date[1];
  l_time->day=	 date[2];
  l_time->hour=  date[3];
  l_time->minute=date[4];
  l_time->second=date[5];
  l_time->second_part=date[6];
  DBUG_RETURN(l_time->time_type=
	      (number_of_fields <= 3 ? TIMESTAMP_DATE : TIMESTAMP_FULL));
}


time_t str_to_timestamp(const char *str,uint length)
{
  TIME l_time;
  long not_used;
  time_t timestamp= 0;

  if (str_to_TIME(str,length,&l_time,0) != TIMESTAMP_NONE &&
      !(timestamp= my_gmt_sec(&l_time, &not_used)))
    current_thd->cuted_fields++;
  
  return timestamp;
}


longlong str_to_datetime(const char *str,uint length,bool fuzzy_date)
{
  TIME l_time;
  if (str_to_TIME(str,length,&l_time,fuzzy_date) == TIMESTAMP_NONE)
    return(0);
  return (longlong) (l_time.year*LL(10000000000) +
		     l_time.month*LL(100000000)+
		     l_time.day*LL(1000000)+
		     l_time.hour*LL(10000)+
		     (longlong) (l_time.minute*100+l_time.second));
}


/*****************************************************************************
** convert a time string to a (ulong) value.
** Can use all full timestamp formats and
** [-] DAYS [H]H:MM:SS, [H]H:MM:SS, [M]M:SS, [H]HMMSS, [M]MSS or [S]S
** There may be an optional [.second_part] after seconds
*****************************************************************************/

bool str_to_time(const char *str,uint length,TIME *l_time)
{
  long date[5],value;
  const char *end=str+length;
  bool found_days,found_hours;
  uint state;

  l_time->neg=0;
  for (; str != end && !isdigit(*str) && *str != '-' ; str++)
    length--;
  if (str != end && *str == '-')
  {
    l_time->neg=1;
    str++;
    length--;
  }
  if (str == end)
    return 1;

  /* Check first if this is a full TIMESTAMP */
  if (length >= 12)
  {						// Probably full timestamp
    if (str_to_TIME(str,length,l_time,1) == TIMESTAMP_FULL)
      return 0;					// Was an ok timestamp
  }

  /* Not a timestamp. Try to get this as a DAYS_TO_SECOND string */
  for (value=0; str != end && isdigit(*str) ; str++)
    value=value*10L + (long) (*str - '0');

  if (*str == ' ')
  {
    while (++str != end && str[0] == ' ') ;
    str--;
  }

  LINT_INIT(state);
  found_days=found_hours=0;
  if ((uint) (end-str) > 1 && (*str == ' ' && isdigit(str[1])))
  {						// days !
    date[0]=value;
    state=1;					// Assume next is hours
    found_days=1;
    str++;					// Skip space;
  }
  else if ((end-str) > 1 && *str == ':' && isdigit(str[1]))
  {
    date[0]=0;					// Assume we found hours
    date[1]=value;
    state=2;
    found_hours=1;
    str++;					// skip ':'
  }
  else
  {
    /* String given as one number; assume HHMMSS format */
    date[0]= 0;
    date[1]= value/10000;
    date[2]= value/100 % 100;
    date[3]= value % 100;
    state=4;
    goto fractional;
  }

  /* Read hours, minutes and seconds */
  for (;;)
  {
    for (value=0; str != end && isdigit(*str) ; str++)
      value=value*10L + (long) (*str - '0');
    date[state++]=value;
    if (state == 4 || (end-str) < 2 || *str != ':' || !isdigit(str[1]))
      break;
    str++;					// Skip ':'
  }

  if (state != 4)
  {						// Not HH:MM:SS
    /* Fix the date to assume that seconds was given */
    if (!found_hours && !found_days)
    {
      bmove_upp((char*) (date+4), (char*) (date+state),
		sizeof(long)*(state-1));
      bzero((char*) date, sizeof(long)*(4-state));
    }
    else
      bzero((char*) (date+state), sizeof(long)*(4-state));
  }

 fractional:
  /* Get fractional second part */
  if ((end-str) >= 2 && *str == '.' && isdigit(str[1]))
  {
    uint field_length=3;
    str++; value=(uint) (uchar) (*str - '0');
    while (++str != end && isdigit(str[0]) && field_length--)
      value=value*10 + (uint) (uchar) (*str - '0');
    date[4]=value;
  }
  else
    date[4]=0;

  /* Some simple checks */
  if (date[2] >= 60 || date[3] >= 60)
  {
    current_thd->cuted_fields++;
    return 1;
  }
  l_time->month=0;
  l_time->day=date[0];
  l_time->hour=date[1];
  l_time->minute=date[2];
  l_time->second=date[3];
  l_time->second_part=date[4];

  /* Check if there is garbage at end of the TIME specification */
  if (str != end && current_thd->count_cuted_fields)
  {
    do
    {
      if (!isspace(*str))
      {
	current_thd->cuted_fields++;
	break;
      }
    } while (++str != end);
  }
  return 0;
}
