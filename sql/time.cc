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

bool parse_datetime_formats(datetime_format_types format_type, 
			    const char *format_str, uint format_length,
			    byte *dt_pos);

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
	  t->minute != (uint) l_time->tm_min);
       loop++)
  {					/* One check should be enough ? */
    /* Get difference in days */
    int days= t->day - l_time->tm_mday;
    if (days < -1)
      days= 1;					// Month has wrapped
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour)) +
	  (long) (60*((int) t->minute - (int) l_time->tm_min)));
    current_timezone+= diff+3600;		// Compensate for -3600 above
    tmp+= (time_t) diff;
    localtime_r(&tmp,&tm_tmp);
    l_time=&tm_tmp;
  }
  /*
    Fix that if we are in the not existing daylight saving time hour
    we move the start of the next real hour
  */
  if (loop == 2 && t->hour != (uint) l_time->tm_hour)
  {
    int days= t->day - l_time->tm_mday;
    if (days < -1)
      days=1;					// Month has wrapped
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour))+
	  (long) (60*((int) t->minute - (int) l_time->tm_min)));
    if (diff == 3600)
      tmp+=3600 - t->minute*60 - t->second;	// Move to next hour
    else if (diff == -3600)
      tmp-=t->minute*60 + t->second;		// Move to previous hour
  }
  *my_timezone= current_timezone;
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

/* Calculate week.  If 'with_year' is not set, then return a week 0-53, where
   0 means that it's the last week of the previous year.
   If 'with_year' is set then the week will always be in the range 1-53 and
   the year out parameter will contain the year for the week */

uint calc_week(TIME *l_time, bool with_year, bool sunday_first_day_of_week,
	       uint *year)
{
  uint days;
  ulong daynr=calc_daynr(l_time->year,l_time->month,l_time->day);
  ulong first_daynr=calc_daynr(l_time->year,1,1);
  uint weekday=calc_weekday(first_daynr,sunday_first_day_of_week);
  *year=l_time->year;
  if (l_time->month == 1 && l_time->day <= 7-weekday &&
      ((!sunday_first_day_of_week && weekday >= 4) ||
       (sunday_first_day_of_week && weekday != 0)))
  {
    /* Last week of the previous year */
    if (!with_year)
      return 0;
    with_year=0;				// Don't check the week again
    (*year)--;
    first_daynr-= (days=calc_days_in_year(*year));
    weekday= (weekday + 53*7- days) % 7;
  }
  if ((sunday_first_day_of_week && weekday != 0) ||
      (!sunday_first_day_of_week && weekday >= 4))
    days= daynr - (first_daynr+ (7-weekday));
  else
    days= daynr - (first_daynr - weekday);
  if (with_year && days >= 52*7)
  {
    /* Check if we are on the first week of the next year (or week 53) */
    weekday= (weekday + calc_days_in_year(*year)) % 7;
    if (weekday < 4)
    {					// We are at first week on next year
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


/*
  Convert a timestamp string to a TIME value.

  SYNOPSIS
    str_to_TIME()
    str			String to parse
    length		Length of string
    l_time		Date is stored here
    fuzzy_date		1 if we should allow dates where one part is zero

  DESCRIPTION
    At least the following formats are recogniced (based on number of digits)
    YYMMDD, YYYYMMDD, YYMMDDHHMMSS, YYYYMMDDHHMMSS
    YY-MM-DD, YYYY-MM-DD, YY-MM-DD HH.MM.SS
    YYYYMMDDTHHMMSS  where T is a the character T (ISO8601)
    Also dates where all parts are zero are allowed

  RETURN VALUES
    TIMESTAMP_NONE	String wasn't a timestamp, like
			[DD [HH:[MM:[SS]]]].fraction
    TIMESTAMP_DATE	DATE string (YY MM and DD parts ok)
    TIMESTAMP_FULL	Full timestamp
*/

timestamp_type
str_to_TIME(const char *str, uint length, TIME *l_time,bool fuzzy_date,THD *thd)
{
  uint field_length= 0, year_length= 0, digits, i, number_of_fields;
  uint date[7], date_len[7];
  uint not_zero_date;
  bool is_internal_format= 0;
  const char *pos;
  const char *end=str+length;
  bool found_delimitier= 0;
  DBUG_ENTER("str_to_TIME");
  DBUG_PRINT("enter",("str: %.*s",length,str));

  // Skip garbage
  for (; str != end && !my_isdigit(&my_charset_latin1, *str) ; str++) ; 
  if (str == end)
    DBUG_RETURN(TIMESTAMP_NONE);
  /*
    Calculate first number of digits.
    If length= 8 or >= 14 then year is of format YYYY.
    (YYYY-MM-DD,  YYYYMMDD, YYYYYMMDDHHMMSS)
  */
  for (pos=str; pos != end && my_isdigit(&my_charset_latin1,*pos) ; pos++) ;
  /* Check for internal format */
  digits= (uint) (pos-str);

  if (pos == end || digits>=12)
  {
    is_internal_format= 1;
    year_length= (digits == 4 || digits == 8 || digits >= 14) ? 4 : 2;
    field_length=year_length-1;
    date_len[0]= year_length;
  }
  not_zero_date= 0;
  for (i=0 ; i < 6 && str != end && my_isdigit(&my_charset_latin1,*str) ; i++)
  {
    if (!is_internal_format)
      date_len[i]= 1;
    uint tmp_value=(uint) (uchar) (*str++ - '0');
    while (str != end && my_isdigit(&my_charset_latin1,str[0]) 
	   && (is_internal_format && field_length-- || !is_internal_format) )
    {
      tmp_value=tmp_value*10 + (uint) (uchar) (*str - '0');
      str++;
      if (!is_internal_format)
	date_len[i]+= 1;
    }
    if (i == 2 && *str == '.')
      DBUG_RETURN(TIMESTAMP_NONE);
    date[i]=tmp_value;
    not_zero_date|= tmp_value;
    if (i == 2 && str != end && *str == 'T')
      str++;					// ISO8601:  CCYYMMDDThhmmss
    else if ( i != 5 ) 				// Skip inter-field delimiters 
    {
      while (str != end && 
             (my_ispunct(&my_charset_latin1,*str) || 
              my_isspace(&my_charset_latin1,*str)))
      {
	// Only allow space between days and hours
	if (my_isspace(&my_charset_latin1,*str) && i != 2)
	  DBUG_RETURN(TIMESTAMP_NONE);
	str++;
	found_delimitier=1;			// Should be a 'normal' date
      }
    }
    if (is_internal_format)
	field_length=1;				// Rest fields can only be 2
  }
  /* Handle second fractions */
  if (i == 6 && (uint) (end-str) >= 2 && *str == '.' && 
      my_isdigit(&my_charset_latin1,str[1]))
  {
    str++;
    uint tmp_value=(uint) (uchar) (*str - '0');
    field_length=5;
    while (str++ != end && my_isdigit(&my_charset_latin1,str[0]) &&
	   field_length--)
      tmp_value=tmp_value*10 + (uint) (uchar) (*str - '0');
    date[6]=tmp_value;
    not_zero_date|= tmp_value;
  }
  else
    date[6]=0;

  while (str != end && (my_ispunct(&my_charset_latin1,*str) || 
			my_isspace(&my_charset_latin1,*str)))
    str++;

  uint add_hours= 0;
  if (!my_strnncoll(&my_charset_latin1, 
		    (const uchar *)str, 2, 
		    (const uchar *)"PM", 2))
    add_hours= 12;

  number_of_fields=i;
  while (i < 6)
    date[i++]=0;

  if (!is_internal_format)
  {
    byte *frm_pos;

    if (number_of_fields  <= 3)
    {
      frm_pos= t_datetime_frm(thd, DATE_FORMAT_TYPE).datetime_format.dt_pos;
      l_time->hour=  0;
      l_time->minute= 0;
      l_time->second= 0;
    }
    else
    {
      frm_pos= t_datetime_frm(thd, DATETIME_FORMAT_TYPE).datetime_format.dt_pos;
      l_time->hour=  date[(int) frm_pos[3]];
      l_time->minute=date[(int) frm_pos[4]];
      l_time->second=date[(int) frm_pos[5]];
      if (frm_pos[6] == 1)
      {
	if (l_time->hour > 12)
	  DBUG_RETURN(WRONG_TIMESTAMP_FULL);
	l_time->hour= l_time->hour%12 + add_hours;
      }
    }

    l_time->year=  date[(int) frm_pos[0]];
    l_time->month= date[(int) frm_pos[1]];
    l_time->day=  date[(int) frm_pos[2]];
    year_length= date_len[(int) frm_pos[0]];
  }
  else
  {
    l_time->year=  date[0];
    l_time->month= date[1];
    l_time->day=  date[2];
    l_time->hour=  date[3];
    l_time->minute=date[4];
    l_time->second=date[5];
  }
  l_time->second_part=date[6];
  l_time->neg= 0;
  if (year_length == 2 && i >=2 && (l_time->month || l_time->day))
    l_time->year+= (l_time->year < YY_PART_YEAR ? 2000 : 1900);


  if (number_of_fields < 3 || l_time->month > 12 ||
      l_time->day > 31 || l_time->hour > 23 ||
      l_time->minute > 59 || l_time->second > 59 ||
      (!fuzzy_date && (l_time->month == 0 || l_time->day == 0)))
  {
    /* Only give warning for a zero date if there is some garbage after */
    if (!not_zero_date)				// If zero date
    {
      for (; str != end ; str++)
      {
	if (!my_isspace(&my_charset_latin1, *str))
	{
	  not_zero_date= 1;			// Give warning
	  break;
	}
      }
    }
    if (not_zero_date)
      thd->cuted_fields++;
    DBUG_RETURN(WRONG_TIMESTAMP_FULL);
  }
  if (str != end && thd->count_cuted_fields)
  {
    for (; str != end ; str++)
    {
      if (!my_isspace(&my_charset_latin1,*str))
      {
	thd->cuted_fields++;
	break;
      }
    }
  }

  DBUG_RETURN(l_time->time_type=
	      (number_of_fields <= 3 ? TIMESTAMP_DATE : TIMESTAMP_FULL));
}


time_t str_to_timestamp(const char *str,uint length, THD *thd)
{
  TIME l_time;
  long not_used;

  if (str_to_TIME(str,length,&l_time,0,thd) <= WRONG_TIMESTAMP_FULL)
    return(0);
  if (l_time.year >= TIMESTAMP_MAX_YEAR || l_time.year < 1900+YY_PART_YEAR)
  {
    thd->cuted_fields++;
    return(0);
  }
  return(my_gmt_sec(&l_time, &not_used));
}


longlong str_to_datetime(const char *str,uint length,bool fuzzy_date, THD *thd)
{
  TIME l_time;
  if (str_to_TIME(str,length,&l_time,fuzzy_date,thd) <= WRONG_TIMESTAMP_FULL)
    return(0);
  return (longlong) (l_time.year*LL(10000000000) +
		     l_time.month*LL(100000000)+
		     l_time.day*LL(1000000)+
		     l_time.hour*LL(10000)+
		     (longlong) (l_time.minute*100+l_time.second));
}


/*
 Convert a time string to a TIME struct.

  SYNOPSIS
   str_to_time()
   str			A string in full TIMESTAMP format or
			[-] DAYS [H]H:MM:SS, [H]H:MM:SS, [M]M:SS, [H]HMMSS,
			[M]MSS or [S]S
			There may be an optional [.second_part] after seconds
   length		Length of str
   l_time		Store result here

   RETURN
     0	ok
     1  error
*/

bool str_to_time(const char *str,uint length,TIME *l_time, THD *thd)
{
  long date[5],value;
  const char *end=str+length;
  bool found_days,found_hours;
  uint state;
  byte *frm_pos= t_datetime_frm(thd, TIME_FORMAT_TYPE).datetime_format.dt_pos;

  l_time->neg=0;
  for (; str != end && 
         !my_isdigit(&my_charset_latin1,*str) && *str != '-' ; str++)
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
    enum timestamp_type tres= str_to_TIME(str,length,l_time,1,thd);
    if (tres == TIMESTAMP_FULL)
      return 0;
    else if (tres == WRONG_TIMESTAMP_FULL)
      return 1;
  }

  /* Not a timestamp. Try to get this as a DAYS_TO_SECOND string */
  for (value=0; str != end && my_isdigit(&my_charset_latin1,*str) ; str++)
    value=value*10L + (long) (*str - '0');

  /* Move to last space */
  if (str != end && *str == ' ')
  {
    while (++str != end && str[0] == ' ')
    {}
    str--;
  }

  LINT_INIT(state);
  found_days=found_hours=0;
  if ((uint) (end-str) > 1 && (*str == ' ' && 
      my_isdigit(&my_charset_latin1,str[1])))
  {						// days !
    date[0]=value;
    state=1;					// Assume next is hours
    found_days=1;
    str++;					// Skip space;
  }
  else if ((end-str) > 1 &&  *str == frm_pos[7] &&
           my_isdigit(&my_charset_latin1,str[1]))
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
    for (value=0; str != end && my_isdigit(&my_charset_latin1,*str) ; str++)
      value=value*10L + (long) (*str - '0');
    date[state++]=value;
    if (state == 4 || (end-str) < 2 || *str != frm_pos[7] ||
	!my_isdigit(&my_charset_latin1,str[1]))
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
  if ((end-str) >= 2 && *str == '.' && my_isdigit(&my_charset_latin1,str[1]))
  {
    uint field_length=5;
    str++; value=(uint) (uchar) (*str - '0');
    while (++str != end && 
           my_isdigit(&my_charset_latin1,str[0]) && 
           field_length--)
      value=value*10 + (uint) (uchar) (*str - '0');
    date[4]=value;
  }
  else
    date[4]=0;

  while (str != end && !my_isalpha(&my_charset_latin1,*str))
    str++;

  if ( (end-str)>= 2 &&
       !my_strnncoll(&my_charset_latin1, 
		     (const uchar *)str, 2, 
		     (const uchar *)"PM", 2) &&
       frm_pos[6] == 1)
  {
      uint days_i= date[1]/24;
      uint hours_i= date[1]%24;
      date[1]= hours_i%12 + 12 + 24*days_i;
  }

  /* Some simple checks */
  if (date[2] >= 60 || date[3] >= 60)
  {
    current_thd->cuted_fields++;
    return 1;
  }
  l_time->month=0;
  l_time->day=date[0];
  l_time->hour=date[frm_pos[3] + 1];
  l_time->minute=date[frm_pos[4] + 1];
  l_time->second=date[frm_pos[5] + 1];
  l_time->second_part=date[4];
  l_time->time_type= TIMESTAMP_TIME;

  /* Check if there is garbage at end of the TIME specification */
  if (str != end && current_thd->count_cuted_fields)
  {
    do
    {
      if (!my_isspace(&my_charset_latin1,*str))
      {
	current_thd->cuted_fields++;
	break;
      }
    } while (++str != end);
  }
  return 0;
}


/*
  Convert a system time structure to TIME
*/

void localtime_to_TIME(TIME *to, struct tm *from)
{
  to->neg=0;
  to->second_part=0;
  to->year=	(int) ((from->tm_year+1900) % 10000);
  to->month=	(int) from->tm_mon+1;
  to->day=	(int) from->tm_mday;
  to->hour=	(int) from->tm_hour;
  to->minute=	(int) from->tm_min;
  to->second=   (int) from->tm_sec;
}

void calc_time_from_sec(TIME *to, long seconds, long microseconds)
{
  long t_seconds;
  to->hour= seconds/3600L;
  t_seconds= seconds%3600L;
  to->minute= t_seconds/60L;
  to->second= t_seconds%60L;
  to->second_part= microseconds;
}


DATETIME_FORMAT *make_format(DATETIME_FORMAT *datetime_format,
			     datetime_format_types format_type,
			     const char *format_str,
			     uint format_length, bool is_alloc)
{
  if (format_length &&
      !parse_datetime_formats(format_type, format_str,
			      format_length, 
			      datetime_format->dt_pos))
  {
    if (is_alloc)
    {
      if (!(datetime_format->format= my_strdup_with_length(format_str,
							   format_length,
							   MYF(0))))
	return 0;
    }
    else
      datetime_format->format= (char *) format_str;
    datetime_format->format_length= format_length;
    return datetime_format;
  }
  return 0;
}


bool parse_datetime_formats(datetime_format_types format_type, 
			    const char *format_str, uint format_length,
			    byte *dt_pos)
{
  uint pos= 0;
  dt_pos[0]= dt_pos[1]= dt_pos[2]= dt_pos[3]= 
             dt_pos[4]= dt_pos[5]= dt_pos[6]= dt_pos[7]= -1;

  const char *ptr=format_str;
  const char *end=ptr+format_length;
  bool need_p= 0;

  for (; ptr != end; ptr++)
  {
    if (*ptr == '%' && ptr+1 != end)
    {
      switch (*++ptr) {
      case 'y':
      case 'Y':
	if (dt_pos[0] > -1)
	  return 1;
	dt_pos[0]= pos;
	break;
      case 'c':
      case 'm':
	if (dt_pos[1] > -1)
	  return 1;
	dt_pos[1]= pos;
	break;
      case 'd':
      case 'e':
	if (dt_pos[2] > -1)
	  return 1;
	dt_pos[2]= pos;
	break;
      case 'H':
      case 'k':
      case 'h':
      case 'I':
      case 'l':
	if (dt_pos[3] > -1)
	  return 1;
	dt_pos[3]= pos;
	need_p= (*ptr == 'h' || *ptr == 'l' || *ptr == 'I');
	break;
      case 'i':
	if (dt_pos[4] > -1)
	  return 1;
	dt_pos[4]= pos;
	break;
      case 's':
      case 'S':
	if (dt_pos[5] > -1)
	  return 1;
	dt_pos[5]= pos;
	break;
      case 'p':
	if (dt_pos[6] > -1)
	  return 1;
	/* %p should be last in format string */
	if (format_type == DATE_FORMAT_TYPE ||
	    (pos != 6 && format_type == DATETIME_FORMAT_TYPE) ||
	    (pos != 3 && format_type == TIME_FORMAT_TYPE))
	  return 1;
	dt_pos[6]= 1;
	break;
      default:
	return 1;
      }
      if (dt_pos[6] == -1)
	pos++;
    }
  }

  if (pos > 5 && format_type == DATETIME_FORMAT_TYPE &&
      (dt_pos[0] + dt_pos[1] + dt_pos[2] + 
       dt_pos[3] + dt_pos[4] + dt_pos[5] != 15) ||
      pos > 2 && format_type == DATE_FORMAT_TYPE && 
      (dt_pos[0] + dt_pos[1] + dt_pos[2] != 3) ||
      pos > 2 && format_type == TIME_FORMAT_TYPE &&
      (dt_pos[3] + dt_pos[4] + dt_pos[5] != 3) ||
      (need_p && dt_pos[6] != 1))
      return 1;

  /*
    Check for valid separators between date/time parst
  */
  uint tmp_len= format_length;
  if (dt_pos[6] == 1)
  {
    end= end - 2;
    if (my_ispunct(&my_charset_latin1, *end) || my_isspace(&my_charset_latin1, *end))
      end--;
    tmp_len= end - format_str;
  }
  switch (format_type) {
  case DATE_FORMAT_TYPE:
  case TIME_FORMAT_TYPE:
    if ((tmp_len == 6 && 
	 !my_strnncoll(&my_charset_bin,
		      (const uchar *) format_str, 6, 
		      (const uchar *) datetime_formats
		      [format_type][INTERNAL_FORMAT], 6)) ||
	tmp_len == 8 &&
	my_ispunct(&my_charset_latin1, *(format_str+2)) &&
	my_ispunct(&my_charset_latin1, *(format_str+5)))
    {
      if (format_type == TIME_FORMAT_TYPE && tmp_len == 8)
      {
	if (*(format_str+2) != *(format_str+5))
	  return 1;
	dt_pos[7]= *(format_str+2);
      }
      return 0;
    }
    break;
  case DATETIME_FORMAT_TYPE:
    if ((tmp_len == 12 && 
	 !my_strnncoll(&my_charset_bin, 
		      (const uchar *) format_str, 12, 
		      (const uchar *) datetime_formats
		      [DATETIME_FORMAT_TYPE][INTERNAL_FORMAT], 12)) ||
	tmp_len == 17 &&
	my_ispunct(&my_charset_latin1, *(format_str+2)) &&
	my_ispunct(&my_charset_latin1, *(format_str+5)) &&
	my_ispunct(&my_charset_latin1, *(format_str+11)) &&
	my_ispunct(&my_charset_latin1, *(format_str+14)) &&
	(my_ispunct(&my_charset_latin1, *(format_str+8)) ||
	 my_isspace(&my_charset_latin1, *(format_str+8))))
      return 0;
    break;
    }
  return 1;
}
