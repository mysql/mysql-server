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


	/* Init some variabels neaded when using my_local_time */
	/* Currently only my_time_zone is inited */

static long my_time_zone=0;
pthread_mutex_t LOCK_timezone;

void init_time(void)
{
  time_t seconds;
  struct tm *l_time,tm_tmp;;
  TIME my_time;

  seconds= (time_t) time((time_t*) 0);
  localtime_r(&seconds,&tm_tmp);
  l_time= &tm_tmp;
  my_time_zone=0;
  my_time.year=		(uint) l_time->tm_year+1900;
  my_time.month=	(uint) l_time->tm_mon+1;
  my_time.day=		(uint) l_time->tm_mday;
  my_time.hour=		(uint) l_time->tm_hour;
  my_time.minute=	(uint) l_time->tm_min;
  my_time.second=		(uint) l_time->tm_sec;
  VOID(my_gmt_sec(&my_time));		/* Init my_time_zone */
}

/*
  Convert current time to sec. since 1970.01.01 
  This code handles also day light saving time.
  The idea is to cache the time zone (including daylight saving time)
  for the next call to make things faster.
  
*/

long my_gmt_sec(TIME *t)
{
  uint loop;
  time_t tmp;
  struct tm *l_time,tm_tmp;
  long diff;

  if (t->hour >= 24)
  {					/* Fix for time-loop */
    t->day+=t->hour/24;
    t->hour%=24;
  }
  pthread_mutex_lock(&LOCK_timezone);
  tmp=(time_t) ((calc_daynr((uint) t->year,(uint) t->month,(uint) t->day) -
		 (long) days_at_timestart)*86400L + (long) t->hour*3600L +
		(long) (t->minute*60 + t->second)) + (time_t) my_time_zone;
  localtime_r(&tmp,&tm_tmp);
  l_time=&tm_tmp;
  for (loop=0;
       loop < 3 &&
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
    my_time_zone+=diff;
    tmp+=(time_t) diff;
    localtime_r(&tmp,&tm_tmp);
    l_time=&tm_tmp;
  }
  /* Fix that if we are in the not existing daylight saving time hour
     we move the start of the next real hour */
  if (loop == 3 && t->hour != (uint) l_time->tm_hour)
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
      tmp-=t->minute*60 + t->second;		// Move to next hour
  }
  if ((my_time_zone >=0 ? my_time_zone: -my_time_zone) > 3600L*12)
    my_time_zone=0;			/* Wrong date */
  pthread_mutex_unlock(&LOCK_timezone);
  return tmp;
} /* my_gmt_sec */


	/* Some functions to calculate dates */

	/* Calculate nr of day since year 0 in new date-system (from 1615) */

long calc_daynr(uint year,uint month,uint day)
{
  long delsum;
  int temp;
  DBUG_ENTER("calc_daynr");

  if (year == 0 && month == 0 && day == 0)
    DBUG_RETURN(0);				/* Skipp errors */
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
  if (l_time->month == 1 && weekday >= 4 && l_time->day <= 7-weekday)
  {
    /* Last week of the previous year */
    if (!with_year)
      return 0;
    with_year=0;				// Don't check the week again
    (*year)--;
    first_daynr-= (days=calc_days_in_year(*year));
    weekday= (weekday + 53*7- days) % 7;
  }
  if (weekday >= 4)
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

/*	find date from string and put it in vektor
	Input: pos = "YYMMDD" OR "YYYYMMDD" in any order or
	"xxxxx YYxxxMMxxxDD xxxx" where xxx is anything exept
	a number. Month or day mustn't exeed 2 digits, year may be 4 digits.
*/


#ifdef NOT_NEEDED

void find_date(string pos,uint *vek,uint flag)
{
  uint length,value;
  string start;
  DBUG_ENTER("find_date");
  DBUG_PRINT("enter",("pos: '%s'  flag: %d",pos,flag));

  bzero((char*) vek,sizeof(int)*4);
  while (*pos && !isdigit(*pos))
    pos++;
  length=(uint) strlen(pos);
  for (uint i=0 ; i< 3; i++)
  {
    start=pos; value=0;
    while (isdigit(pos[0]) &&
	   ((pos-start) < 2 || ((pos-start) < 4 && length >= 8 &&
				!(flag & 3))))
    {
      value=value*10 + (uint) (uchar) (*pos - '0');
      pos++;
    }
    vek[flag & 3]=value; flag>>=2;
    while (*pos && (ispunct(*pos) || isspace(*pos)))
      pos++;
  }
  DBUG_PRINT("exit",("year: %d  month: %d  day: %d",vek[0],vek[1],vek[2]));
  DBUG_VOID_RETURN;
} /* find_date */


	/* Outputs YYMMDD if input year < 100 or YYYYMMDD else */

static long calc_daynr_from_week(uint year,uint week,uint day)
{
  long daynr;
  int weekday;

  daynr=calc_daynr(year,1,1);
  if ((weekday= calc_weekday(daynr,0)) >= 3)
    daynr+= (7-weekday);
  else
    daynr-=weekday;

  return (daynr+week*7+day-8);
}

void convert_week_to_date(string date,uint flag,uint *res_length)
{
  string format;
  uint year,vek[4];

  find_date(date,vek,(uint) (1*4+2*16));		/* YY-WW-DD */
  year=vek[0];

  get_date_from_daynr(calc_daynr_from_week(vek[0],vek[1],vek[2]),
		      &vek[0],&vek[1],&vek[2]);
  *res_length=8;
  format="%04d%02d%02d";
  if (year < 100)
  {
    vek[0]= vek[0]%100;
    *res_length=6;
    format="%02d%02d%02d";
  }
  sprintf(date,format,vek[flag & 3],vek[(flag >> 2) & 3],
	  vek[(flag >> 4) & 3]);
  return;
}

	/* returns YYWWDD or YYYYWWDD according to input year */
	/* flag only reflects format of input date */

void convert_date_to_week(string date,uint flag,uint *res_length)
{
  uint vek[4],weekday,days,year,week,day;
  long daynr,first_daynr;
  char buff[256],*format;

  if (! date[0])
  {
    get_date(buff,0,0L);			/* Use current date */
    find_date(buff+2,vek,(uint) (1*4+2*16));	/* YY-MM-DD */
  }
  else
    find_date(date,vek,flag);

  year= vek[0];
  daynr=      calc_daynr(year,vek[1],vek[2]);
  first_daynr=calc_daynr(year,1,1);

	/* Caculate year and first daynr of year */
  if (vek[1] == 1 && (weekday=calc_weekday(first_daynr,0)) >= 3 &&
      vek[2] <= 7-weekday)
  {
    if (!year--)
      year=99;
    first_daynr=first_daynr-calc_days_in_year(year);
  }
  else if (vek[1] == 12 &&
	   (weekday=calc_weekday(first_daynr+calc_days_in_year(year)),0) < 3 &&
	   vek[2] > 31-weekday)
  {
    first_daynr=first_daynr+calc_days_in_year(year);
    if (year++ == 99)
      year=0;
  }

	/* Calulate daynr of first day of week 1 */
  if ((weekday= calc_weekday(first_daynr,0)) >= 3)
    first_daynr+= (7-weekday);
  else
    first_daynr-=weekday;

  days=(int) (daynr-first_daynr);
  week=days/7+1 ; day=calc_weekday(daynr,0)+1;

  *res_length=8;
  format="%04d%02d%02d";
  if (year < 100)
  {
    *res_length=6;
    format="%02d%02d%02d";
  }
  sprintf(date,format,year,week,day);
  return;
}

#endif

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

#ifdef NOT_NEEDED

ulong add_to_period(ulong period,int months)
{
  if (period == 0L)
    return 0L;
  return convert_month_to_period(convert_period_to_month(period)+months);
}
#endif


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
  bool date_used=0;
  const char *pos;
  const char *end=str+length;
  DBUG_ENTER("str_to_TIME");
  DBUG_PRINT("enter",("str: %.*s",length,str));

  for (; !isdigit(*str) && str != end ; str++) ; // Skipp garbage
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
  for (i=0 ; i < 6 && str != end && isdigit(*str) ; i++)
  {
    uint tmp_value=(uint) (uchar) (*str++ - '0');
    while (str != end && isdigit(str[0]) && field_length--)
    {
      tmp_value=tmp_value*10 + (uint) (uchar) (*str - '0');
      str++;
    }
    if ((date[i]=tmp_value))
      date_used=1;				// Found something
    if (i == 2 && str != end && *str == 'T')
      str++;    // ISO8601:  CCYYMMDDThhmmss
    else
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
  }
  else
    date[6]=0;

  if (year_length == 2)
    date[0]+= (date[0] < YY_PART_YEAR ? 2000 : 1900);
  number_of_fields=i;
  while (i < 6)
    date[i++]=0;
  if (number_of_fields < 3 || !date_used || date[1] > 12 ||
      date[2] > 31 || date[3] > 23 || date[4] > 59 || date[5] > 59 ||
      !fuzzy_date && (date[1] == 0 || date[2] == 0))
  {
    current_thd->cuted_fields++;
    DBUG_RETURN(TIMESTAMP_NONE);
  }
  if (str != end && current_thd->count_cuted_fields)
  {
    for ( ; str != end ; str++)
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
  if (str_to_TIME(str,length,&l_time,0) == TIMESTAMP_NONE)
    return(0);
  if (l_time.year >= TIMESTAMP_MAX_YEAR || l_time.year < 1900+YY_PART_YEAR)
  {
    current_thd->cuted_fields++;
    return(0);
  }
  return(my_gmt_sec(&l_time));
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
  for (; !isdigit(*str) && *str != '-' && str != end ; str++)
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
    str++;					// Skipp space;
  }
  else if ((end-str) > 1 && *str == ':' && isdigit(str[1]))
  {
    date[0]=0;					// Assume we found hours
    date[1]=value;
    state=2;
    found_hours=1;
    str++;					// skipp ':'
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
    str++;					// Skipp ':'
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
