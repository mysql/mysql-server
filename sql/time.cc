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


	/* Some functions to calculate dates */

#ifndef TESTTIME
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


/*
  Convert a timestamp string to a TIME value and produce a warning 
  if string was truncated during conversion.

  NOTE
    See description of str_to_datetime() for more information.
*/
timestamp_type
str_to_datetime_with_warn(const char *str, uint length, TIME *l_time,
                          uint flags)
{
  int was_cut;
  timestamp_type ts_type= str_to_datetime(str, length, l_time, flags, &was_cut);
  if (was_cut)
    make_truncated_value_warning(current_thd, str, length, ts_type);
  return ts_type;
}


/*
  Convert a datetime from broken-down TIME representation to corresponding 
  TIMESTAMP value.

  SYNOPSIS
    TIME_to_timestamp()
      thd             - current thread
      t               - datetime in broken-down representation, 
      in_dst_time_gap - pointer to bool which is set to true if t represents
                        value which doesn't exists (falls into the spring 
                        time-gap) or to false otherwise.
   
  RETURN
     Number seconds in UTC since start of Unix Epoch corresponding to t.
     0 - t contains datetime value which is out of TIMESTAMP range.
     
*/
my_time_t TIME_to_timestamp(THD *thd, const TIME *t, bool *in_dst_time_gap)
{
  my_time_t timestamp;

  *in_dst_time_gap= 0;

  if (t->year < TIMESTAMP_MAX_YEAR && t->year > TIMESTAMP_MIN_YEAR ||
      t->year == TIMESTAMP_MAX_YEAR && t->month == 1 && t->day == 1 ||
      t->year == TIMESTAMP_MIN_YEAR && t->month == 12 && t->day == 31)
  {
    thd->time_zone_used= 1;
    timestamp= thd->variables.time_zone->TIME_to_gmt_sec(t, in_dst_time_gap);
    if (timestamp >= TIMESTAMP_MIN_VALUE && timestamp <= TIMESTAMP_MAX_VALUE)
      return timestamp;
  }

  /* If we are here we have range error. */
  return(0);
}


/*
  Convert a time string to a TIME struct and produce a warning
  if string was cut during conversion.

  NOTE
    See str_to_time() for more info.
*/
bool
str_to_time_with_warn(const char *str, uint length, TIME *l_time)
{
  int was_cut;
  bool ret_val= str_to_time(str, length, l_time, &was_cut);
  if (was_cut)
    make_truncated_value_warning(current_thd, str, length, MYSQL_TIMESTAMP_TIME);
  return ret_val;
}


/*
  Convert datetime value specified as number to broken-down TIME 
  representation and form value of DATETIME type as side-effect.

  SYNOPSIS
    number_to_TIME()
      nr         - datetime value as number 
      time_res   - pointer for structure for broken-down representation
      fuzzy_date - indicates whenever we allow fuzzy dates
      was_cut    - set ot 1 if there was some kind of error during 
                   conversion or to 0 if everything was OK.
  
  DESCRIPTION
    Convert a datetime value of formats YYMMDD, YYYYMMDD, YYMMDDHHMSS, 
    YYYYMMDDHHMMSS to broken-down TIME representation. Return value in
    YYYYMMDDHHMMSS format as side-effect.
  
    This function also checks if datetime value fits in DATETIME range.

  RETURN VALUE
    Datetime value in YYYYMMDDHHMMSS format.
    If input value is not valid datetime value then 0 is returned. 
*/

longlong number_to_TIME(longlong nr, TIME *time_res, bool fuzzy_date, 
                        int *was_cut)
{
  long part1,part2;

  *was_cut= 0;
  
  if (nr == LL(0) || nr >= LL(10000101000000))
    goto ok;
  if (nr < 101)
    goto err;
  if (nr <= (YY_PART_YEAR-1)*10000L+1231L)
  {
    nr= (nr+20000000L)*1000000L;                 // YYMMDD, year: 2000-2069
    goto ok;
  }
  if (nr < (YY_PART_YEAR)*10000L+101L)
    goto err;
  if (nr <= 991231L)
  {
    nr= (nr+19000000L)*1000000L;                 // YYMMDD, year: 1970-1999
    goto ok;
  }
  if (nr < 10000101L)
    goto err;
  if (nr <= 99991231L)
  {
    nr= nr*1000000L;
    goto ok;
  }
  if (nr < 101000000L)
    goto err;
  if (nr <= (YY_PART_YEAR-1)*LL(10000000000)+LL(1231235959))
  {
    nr= nr+LL(20000000000000);                   // YYMMDDHHMMSS, 2000-2069
    goto ok;
  }
  if (nr <  YY_PART_YEAR*LL(10000000000)+ LL(101000000))
    goto err;
  if (nr <= LL(991231235959))
    nr= nr+LL(19000000000000);		// YYMMDDHHMMSS, 1970-1999

 ok:
  part1=(long) (nr/LL(1000000));
  part2=(long) (nr - (longlong) part1*LL(1000000));
  time_res->year=  (int) (part1/10000L);  part1%=10000L;
  time_res->month= (int) part1 / 100;
  time_res->day=   (int) part1 % 100;
  time_res->hour=  (int) (part2/10000L);  part2%=10000L;
  time_res->minute=(int) part2 / 100;
  time_res->second=(int) part2 % 100;
    
  if (time_res->year <= 9999 && time_res->month <= 12 && 
      time_res->day <= 31 && time_res->hour <= 23 && 
      time_res->minute <= 59 && time_res->second <= 59 &&
      (fuzzy_date || (time_res->month != 0 && time_res->day != 0) || nr==0))
    return nr;
  
 err:
  
  *was_cut= 1;
  return LL(0);
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


/*
  Parse a format string specification

  SYNOPSIS
    parse_date_time_format()
    format_type		Format of string (time, date or datetime)
    format_str		String to parse
    format_length	Length of string
    date_time_format	Format to fill in

  NOTES
    Fills in date_time_format->positions for all date time parts.

    positions marks the position for a datetime element in the format string.
    The position array elements are in the following order:
    YYYY-DD-MM HH-MM-DD.FFFFFF AM
    0    1  2  3  4  5  6      7

    If positions[0]= 5, it means that year will be the forth element to
    read from the parsed date string.

  RETURN
    0	ok
    1	error
*/

bool parse_date_time_format(timestamp_type format_type, 
			    const char *format, uint format_length,
			    DATE_TIME_FORMAT *date_time_format)
{
  uint offset= 0, separators= 0;
  const char *ptr= format, *format_str;
  const char *end= ptr+format_length;
  uchar *dt_pos= date_time_format->positions;
  /* need_p is set if we are using AM/PM format */
  bool need_p= 0, allow_separator= 0;
  ulong part_map= 0, separator_map= 0;
  const char *parts[16];

  date_time_format->time_separator= 0;
  date_time_format->flag= 0;			// For future

  /*
    Fill position with 'dummy' arguments to found out if a format tag is
    used twice (This limit's the format to 255 characters, but this is ok)
  */
  dt_pos[0]= dt_pos[1]= dt_pos[2]= dt_pos[3]=
    dt_pos[4]= dt_pos[5]= dt_pos[6]= dt_pos[7]= 255;

  for (; ptr != end; ptr++)
  {
    if (*ptr == '%' && ptr+1 != end)
    {
      uint position;
      LINT_INIT(position);
      switch (*++ptr) {
      case 'y':					// Year
      case 'Y':
	position= 0;
	break;
      case 'c':					// Month
      case 'm':
	position= 1;
	break;
      case 'd':
      case 'e':
	position= 2;
	break;
      case 'h':
      case 'I':
      case 'l':
	need_p= 1;				// Need AM/PM
	/* Fall through */
      case 'k':
      case 'H':
	position= 3;
	break;
      case 'i':
	position= 4;
	break;
      case 's':
      case 'S':
	position= 5;
	break;
      case 'f':
	position= 6;
	if (dt_pos[5] != offset-1 || ptr[-2] != '.')
	  return 1;				// Wrong usage of %f
	break;
      case 'p':					// AM/PM
	if (offset == 0)			// Can't be first
	  return 0;
	position= 7;
	break;
      default:
	return 1;				// Unknown controll char
      }
      if (dt_pos[position] != 255)		// Don't allow same tag twice
	return 1;
      parts[position]= ptr-1;

      /*
	If switching from time to date, ensure that all time parts
	are used
      */
      if (part_map && position <= 2 && !(part_map & (1 | 2 | 4)))
	offset=5;
      part_map|= (ulong) 1 << position;
      dt_pos[position]= offset++;
      allow_separator= 1;
    }
    else
    {
      /*
	Don't allow any characters in format as this could easily confuse
	the date reader
      */
      if (!allow_separator)
	return 1;				// No separator here
      allow_separator= 0;			// Don't allow two separators
      separators++;
      /* Store in separator_map which parts are punct characters */
      if (my_ispunct(&my_charset_latin1, *ptr))
	separator_map|= (ulong) 1 << (offset-1);
      else if (!my_isspace(&my_charset_latin1, *ptr))
	return 1;
    }
  }

  /* If no %f, specify it after seconds.  Move %p up, if necessary */
  if ((part_map & 32) && !(part_map & 64))
  {
    dt_pos[6]= dt_pos[5] +1;
    parts[6]= parts[5];				// For later test in (need_p)
    if (dt_pos[6] == dt_pos[7])			// Move %p one step up if used
      dt_pos[7]++;
  }

  /*
    Check that we have not used a non legal format specifier and that all
    format specifiers have been used

    The last test is to ensure that %p is used if and only if
    it's needed.
  */
  if ((format_type == MYSQL_TIMESTAMP_DATETIME &&
       !test_all_bits(part_map, (1 | 2 | 4 | 8 | 16 | 32))) ||
      (format_type == MYSQL_TIMESTAMP_DATE && part_map != (1 | 2 | 4)) ||
      (format_type == MYSQL_TIMESTAMP_TIME &&
       !test_all_bits(part_map, 8 | 16 | 32)) ||
      !allow_separator ||			// %option should be last
      (need_p && dt_pos[6] +1 != dt_pos[7]) ||
      (need_p ^ (dt_pos[7] != 255)))
    return 1;

  if (dt_pos[6] != 255)				// If fractional seconds
  {
    /* remove fractional seconds from later tests */
    uint pos= dt_pos[6] -1;
    /* Remove separator before %f from sep map */
    separator_map= ((separator_map & ((ulong) (1 << pos)-1)) |
		    ((separator_map & ~((ulong) (1 << pos)-1)) >> 1));
    if (part_map & 64)			      
    {
      separators--;				// There is always a separator
      need_p= 1;				// force use of separators
    }
  }

  /*
    Remove possible separator before %p from sep_map
    (This can either be at position 3, 4, 6 or 7) h.m.d.%f %p
  */
  if (dt_pos[7] != 255)
  {
    if (need_p && parts[7] != parts[6]+2)
      separators--;
  }     
  /*
    Calculate if %p is in first or last part of the datetime field

    At this point we have either %H-%i-%s %p 'year parts' or
    'year parts' &H-%i-%s %p" as %f was removed above
  */
  offset= dt_pos[6] <= 3 ? 3 : 6;
  /* Remove separator before %p from sep map */
  separator_map= ((separator_map & ((ulong) (1 << offset)-1)) |
		  ((separator_map & ~((ulong) (1 << offset)-1)) >> 1));

  format_str= 0;
  switch (format_type) {
  case MYSQL_TIMESTAMP_DATE:
    format_str= known_date_time_formats[INTERNAL_FORMAT].date_format;
    /* fall through */
  case MYSQL_TIMESTAMP_TIME:
    if (!format_str)
      format_str=known_date_time_formats[INTERNAL_FORMAT].time_format;

    /*
      If there is no separators, allow the internal format as we can read
      this.  If separators are used, they must be between each part
    */
    if (format_length == 6 && !need_p &&
	!my_strnncoll(&my_charset_bin,
		      (const uchar *) format, 6, 
		      (const uchar *) format_str, 6))
      return 0;
    if (separator_map == (1 | 2))
    {
      if (format_type == MYSQL_TIMESTAMP_TIME)
      {
	if (*(format+2) != *(format+5))
	  break;				// Error
	/* Store the character used for time formats */
	date_time_format->time_separator= *(format+2);
      }
      return 0;
    }
    break;
  case MYSQL_TIMESTAMP_DATETIME:
    /*
      If there is no separators, allow the internal format as we can read
      this.  If separators are used, they must be between each part.
      Between DATE and TIME we also allow space as separator
    */
    if ((format_length == 12 && !need_p &&
	 !my_strnncoll(&my_charset_bin, 
		       (const uchar *) format, 12,
		       (const uchar*) known_date_time_formats[INTERNAL_FORMAT].datetime_format,
		       12)) ||
	(separators == 5 && separator_map == (1 | 2 | 8 | 16)))
      return 0;
    break;
  default:
    DBUG_ASSERT(1);
    break;
  }
  return 1;					// Error
}


/*
  Create a DATE_TIME_FORMAT object from a format string specification

  SYNOPSIS
    date_time_format_make()
    format_type		Format to parse (time, date or datetime)
    format_str		String to parse
    format_length	Length of string

  NOTES
    The returned object should be freed with my_free()

  RETURN
    NULL ponter:	Error
    new object
*/

DATE_TIME_FORMAT
*date_time_format_make(timestamp_type format_type,
		       const char *format_str, uint format_length)
{
  DATE_TIME_FORMAT tmp;

  if (format_length && format_length < 255 &&
      !parse_date_time_format(format_type, format_str,
			      format_length, &tmp))
  {
    tmp.format.str=    (char*) format_str;
    tmp.format.length= format_length;
    return date_time_format_copy((THD *)0, &tmp);
  }
  return 0;
}


/*
  Create a copy of a DATE_TIME_FORMAT object

  SYNOPSIS
    date_and_time_format_copy()
    thd			Set if variable should be allocated in thread mem
    format		format to copy

  NOTES
    The returned object should be freed with my_free()

  RETURN
    NULL ponter:	Error
    new object
*/

DATE_TIME_FORMAT *date_time_format_copy(THD *thd, DATE_TIME_FORMAT *format)
{
  DATE_TIME_FORMAT *new_format;
  ulong length= sizeof(*format) + format->format.length + 1;

  if (thd)
    new_format= (DATE_TIME_FORMAT *) thd->alloc(length);
  else
    new_format=  (DATE_TIME_FORMAT *) my_malloc(length, MYF(MY_WME));
  if (new_format)
  {
    /* Put format string after current pos */
    new_format->format.str= (char*) (new_format+1);
    memcpy((char*) new_format->positions, (char*) format->positions,
	   sizeof(format->positions));
    new_format->time_separator= format->time_separator;
    /* We make the string null terminated for easy printf in SHOW VARIABLES */
    memcpy((char*) new_format->format.str, format->format.str,
	   format->format.length);
    new_format->format.str[format->format.length]= 0;
    new_format->format.length= format->format.length;
  }
  return new_format;
}


KNOWN_DATE_TIME_FORMAT known_date_time_formats[6]=
{
  {"USA", "%m.%d.%Y", "%Y-%m-%d %H.%i.%s", "%h:%i:%s %p" },
  {"JIS", "%Y-%m-%d", "%Y-%m-%d %H:%i:%s", "%H:%i:%s" },
  {"ISO", "%Y-%m-%d", "%Y-%m-%d %H:%i:%s", "%H:%i:%s" },
  {"EUR", "%d.%m.%Y", "%Y-%m-%d %H.%i.%s", "%H.%i.%s" },
  {"INTERNAL", "%Y%m%d",   "%Y%m%d%H%i%s", "%H%i%s" },
  { 0, 0, 0, 0 }
};


/*
   Return format string according format name.
   If name is unknown, result is NULL
*/

const char *get_date_time_format_str(KNOWN_DATE_TIME_FORMAT *format,
				     timestamp_type type)
{
  switch (type) {
  case MYSQL_TIMESTAMP_DATE:
    return format->date_format;
  case MYSQL_TIMESTAMP_DATETIME:
    return format->datetime_format;
  case MYSQL_TIMESTAMP_TIME:
    return format->time_format;
  default:
    DBUG_ASSERT(0);				// Impossible
    return 0;
  }
}

/****************************************************************************
  Functions to create default time/date/datetime strings
 
  NOTE:
    For the moment the DATE_TIME_FORMAT argument is ignored becasue
    MySQL doesn't support comparing of date/time/datetime strings that
    are not in arbutary order as dates are compared as strings in some
    context)
    This functions don't check that given TIME structure members are
    in valid range. If they are not, return value won't reflect any 
    valid date either. Additionally, make_time doesn't take into
    account time->day member: it's assumed that days have been converted
    to hours already.
****************************************************************************/

void make_time(const DATE_TIME_FORMAT *format __attribute__((unused)),
               const TIME *l_time, String *str)
{
  uint length= (uint) my_time_to_str(l_time, (char*) str->ptr());
  str->length(length);
  str->set_charset(&my_charset_bin);
}


void make_date(const DATE_TIME_FORMAT *format __attribute__((unused)),
               const TIME *l_time, String *str)
{
  uint length= (uint) my_date_to_str(l_time, (char*) str->ptr());
  str->length(length);
  str->set_charset(&my_charset_bin);
}


void make_datetime(const DATE_TIME_FORMAT *format __attribute__((unused)),
                   const TIME *l_time, String *str)
{
  uint length= (uint) my_datetime_to_str(l_time, (char*) str->ptr());
  str->length(length);
  str->set_charset(&my_charset_bin);
}


void make_truncated_value_warning(THD *thd, const char *str_val,
				  uint str_length, timestamp_type time_type)
{
  char warn_buff[MYSQL_ERRMSG_SIZE];
  const char *type_str;

  char buff[128];
  String str(buff,(uint32) sizeof(buff), system_charset_info);
  str.length(0);
  str.append(str_val, str_length);
  str.append('\0');

  switch (time_type) {
    case MYSQL_TIMESTAMP_DATE: 
      type_str= "date";
      break;
    case MYSQL_TIMESTAMP_TIME:
      type_str= "time";
      break;
    case MYSQL_TIMESTAMP_DATETIME:  // FALLTHROUGH
    default:
      type_str= "datetime";
      break;
  }
  sprintf(warn_buff, ER(ER_TRUNCATED_WRONG_VALUE),
	  type_str, str.ptr());
  push_warning_printf(thd, MYSQL_ERROR::WARN_LEVEL_WARN,
		      ER_TRUNCATED_WRONG_VALUE, warn_buff);
}


/* Convert time value to integer in YYYYMMDDHHMMSS format */

ulonglong TIME_to_ulonglong_datetime(const TIME *time)
{
  return ((ulonglong) (time->year * 10000UL +
                       time->month * 100UL +
                       time->day) * ULL(1000000) +
          (ulonglong) (time->hour * 10000UL +
                       time->minute * 100UL +
                       time->second));
}


/* Convert TIME value to integer in YYYYMMDD format */

ulonglong TIME_to_ulonglong_date(const TIME *time)
{
  return (ulonglong) (time->year * 10000UL + time->month * 100UL + time->day);
}


/*
  Convert TIME value to integer in HHMMSS format.
  This function doesn't take into account time->day member:
  it's assumed that days have been converted to hours already.
*/

ulonglong TIME_to_ulonglong_time(const TIME *time)
{
  return (ulonglong) (time->hour * 10000UL +
                      time->minute * 100UL +
                      time->second);
}


/*
  Convert struct TIME (date and time split into year/month/day/hour/...
  to a number in format YYYYMMDDHHMMSS (DATETIME),
  YYYYMMDD (DATE)  or HHMMSS (TIME).
  
  SYNOPSIS
    TIME_to_ulonglong()

  DESCRIPTION
    The function is used when we need to convert value of time item
    to a number if it's used in numeric context, i. e.:
    SELECT NOW()+1, CURDATE()+0, CURTIMIE()+0;
    SELECT ?+1;

  NOTE
    This function doesn't check that given TIME structure members are
    in valid range. If they are not, return value won't reflect any 
    valid date either.
*/

ulonglong TIME_to_ulonglong(const TIME *time)
{
  switch (time->time_type) {
  case MYSQL_TIMESTAMP_DATETIME:
    return TIME_to_ulonglong_datetime(time);
  case MYSQL_TIMESTAMP_DATE:
    return TIME_to_ulonglong_date(time);
  case MYSQL_TIMESTAMP_TIME:
    return TIME_to_ulonglong_time(time);
  case MYSQL_TIMESTAMP_NONE:
  case MYSQL_TIMESTAMP_ERROR:
    return ULL(0);
  default:
    DBUG_ASSERT(0);
  }
  return 0;
}

#endif
