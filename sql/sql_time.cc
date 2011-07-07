/* Copyright (c) 2000, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */


/* Functions to handle date and time */

#include "sql_priv.h"
#include "unireg.h"                      // REQUIRED by other includes
#include "sql_time.h"
#include "tztime.h"                             // struct Time_zone
#include "sql_class.h"   // THD, MODE_INVALID_DATES, MODE_NO_ZERO_DATE
#include <m_ctype.h>


	/* Some functions to calculate dates */

#ifndef TESTTIME

/*
  Name description of interval names used in statements.

  'interval_type_to_name' is ordered and sorted on interval size and
  interval complexity.
  Order of elements in 'interval_type_to_name' should correspond to 
  the order of elements in 'interval_type' enum
  
  See also interval_type, interval_names
*/

LEX_STRING interval_type_to_name[INTERVAL_LAST] = {
  { C_STRING_WITH_LEN("YEAR")},
  { C_STRING_WITH_LEN("QUARTER")},
  { C_STRING_WITH_LEN("MONTH")},
  { C_STRING_WITH_LEN("WEEK")},
  { C_STRING_WITH_LEN("DAY")},
  { C_STRING_WITH_LEN("HOUR")},
  { C_STRING_WITH_LEN("MINUTE")},
  { C_STRING_WITH_LEN("SECOND")},
  { C_STRING_WITH_LEN("MICROSECOND")},
  { C_STRING_WITH_LEN("YEAR_MONTH")},
  { C_STRING_WITH_LEN("DAY_HOUR")},
  { C_STRING_WITH_LEN("DAY_MINUTE")},
  { C_STRING_WITH_LEN("DAY_SECOND")},
  { C_STRING_WITH_LEN("HOUR_MINUTE")},
  { C_STRING_WITH_LEN("HOUR_SECOND")},
  { C_STRING_WITH_LEN("MINUTE_SECOND")},
  { C_STRING_WITH_LEN("DAY_MICROSECOND")},
  { C_STRING_WITH_LEN("HOUR_MICROSECOND")},
  { C_STRING_WITH_LEN("MINUTE_MICROSECOND")},
  { C_STRING_WITH_LEN("SECOND_MICROSECOND")}
}; 

	/* Calc weekday from daynr */
	/* Returns 0 for monday, 1 for tuesday .... */

int calc_weekday(long daynr,bool sunday_first_day_of_week)
{
  DBUG_ENTER("calc_weekday");
  DBUG_RETURN ((int) ((daynr + 5L + (sunday_first_day_of_week ? 1L : 0L)) % 7));
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

uint calc_week(MYSQL_TIME *l_time, uint week_behaviour, uint *year)
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
	((first_weekday && weekday != 0) ||
	 (!first_weekday && weekday >= 4)))
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
    if ((!first_weekday && weekday < 4) ||
	(first_weekday && weekday == 0))
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
  Convert a string to 8-bit representation,
  for use in str_to_time/str_to_date/str_to_date.
  
  In the future to_ascii() can be extended to convert
  non-ASCII digits to ASCII digits
  (for example, ARABIC-INDIC, DEVANAGARI, BENGALI, and so on)
  so DATE/TIME/DATETIME values understand digits in the
  respected scripts.
*/
static uint
to_ascii(CHARSET_INFO *cs,
         const char *src, uint src_length,
         char *dst, uint dst_length)
                     
{
  int cnvres;
  my_wc_t wc;
  const char *srcend= src + src_length;
  char *dst0= dst, *dstend= dst + dst_length - 1;
  while (dst < dstend &&
         (cnvres= (cs->cset->mb_wc)(cs, &wc,
                                    (const uchar*) src,
                                    (const uchar*) srcend)) > 0 &&
         wc < 128)
  {
    src+= cnvres;
    *dst++= wc;
  }
  *dst= '\0';
  return dst - dst0;
}


/* Character set-aware version of str_to_time() */
bool str_to_time(CHARSET_INFO *cs, const char *str,uint length,
                 MYSQL_TIME *l_time, int *warning)
{
  char cnv[32];
  if ((cs->state & MY_CS_NONASCII) != 0)
  {
    length= to_ascii(cs, str, length, cnv, sizeof(cnv));
    str= cnv;
  }
  return str_to_time(str, length, l_time, warning);
}


/* Character set-aware version of str_to_datetime() */
timestamp_type str_to_datetime(CHARSET_INFO *cs,
                               const char *str, uint length,
                               MYSQL_TIME *l_time, uint flags, int *was_cut)
{
  char cnv[32];
  if ((cs->state & MY_CS_NONASCII) != 0)
  {
    length= to_ascii(cs, str, length, cnv, sizeof(cnv));
    str= cnv;
  }
  return str_to_datetime(str, length, l_time, flags, was_cut);
}


/*
  Convert a timestamp string to a MYSQL_TIME value and produce a warning 
  if string was truncated during conversion.

  NOTE
    See description of str_to_datetime() for more information.
*/

timestamp_type
str_to_datetime_with_warn(CHARSET_INFO *cs,
                          const char *str, uint length, MYSQL_TIME *l_time,
                          uint flags)
{
  int was_cut;
  THD *thd= current_thd;
  timestamp_type ts_type;
  
  ts_type= str_to_datetime(cs, str, length, l_time,
                           (flags | (thd->variables.sql_mode &
                                     (MODE_INVALID_DATES |
                                      MODE_NO_ZERO_DATE))),
                           &was_cut);
  if (was_cut || ts_type <= MYSQL_TIMESTAMP_ERROR)
    make_truncated_value_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                                 str, length, ts_type,  NullS);
  return ts_type;
}


/*
  Convert a datetime from broken-down MYSQL_TIME representation to corresponding 
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
my_time_t TIME_to_timestamp(THD *thd, const MYSQL_TIME *t, my_bool *in_dst_time_gap)
{
  my_time_t timestamp;

  *in_dst_time_gap= 0;
  thd->time_zone_used= 1;

  timestamp= thd->variables.time_zone->TIME_to_gmt_sec(t, in_dst_time_gap);
  if (timestamp)
  {
    return timestamp;
  }

  /* If we are here we have range error. */
  return(0);
}


/*
  Convert a time string to a MYSQL_TIME struct and produce a warning
  if string was cut during conversion.

  NOTE
    See str_to_time() for more info.
*/
bool
str_to_time_with_warn(CHARSET_INFO *cs,
                      const char *str, uint length, MYSQL_TIME *l_time)
{
  int warning;
  bool ret_val= str_to_time(str, length, l_time, &warning);
  if (ret_val || warning)
    make_truncated_value_warning(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                                 str, length, MYSQL_TIMESTAMP_TIME, NullS);
  return ret_val;
}


/*
  Convert a system time structure to TIME
*/

void localtime_to_TIME(MYSQL_TIME *to, struct tm *from)
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

void calc_time_from_sec(MYSQL_TIME *to, long seconds, long microseconds)
{
  long t_seconds;
  // to->neg is not cleared, it may already be set to a useful value
  to->time_type= MYSQL_TIMESTAMP_TIME;
  to->year= 0;
  to->month= 0;
  to->day= 0;
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
    This functions don't check that given MYSQL_TIME structure members are
    in valid range. If they are not, return value won't reflect any 
    valid date either. Additionally, make_time doesn't take into
    account time->day member: it's assumed that days have been converted
    to hours already.
****************************************************************************/

void make_time(const DATE_TIME_FORMAT *format __attribute__((unused)),
               const MYSQL_TIME *l_time, String *str)
{
  uint length= (uint) my_time_to_str(l_time, (char*) str->ptr());
  str->length(length);
  str->set_charset(&my_charset_numeric);
}


void make_date(const DATE_TIME_FORMAT *format __attribute__((unused)),
               const MYSQL_TIME *l_time, String *str)
{
  uint length= (uint) my_date_to_str(l_time, (char*) str->ptr());
  str->length(length);
  str->set_charset(&my_charset_numeric);
}


void make_datetime(const DATE_TIME_FORMAT *format __attribute__((unused)),
                   const MYSQL_TIME *l_time, String *str)
{
  uint length= (uint) my_datetime_to_str(l_time, (char*) str->ptr());
  str->length(length);
  str->set_charset(&my_charset_numeric);
}


void make_truncated_value_warning(THD *thd, MYSQL_ERROR::enum_warning_level level,
                                  const char *str_val,
				  uint str_length, timestamp_type time_type,
                                  const char *field_name)
{
  char warn_buff[MYSQL_ERRMSG_SIZE];
  const char *type_str;
  CHARSET_INFO *cs= &my_charset_latin1;
  char buff[128];
  String str(buff,(uint32) sizeof(buff), system_charset_info);
  str.copy(str_val, str_length, system_charset_info);
  str[str_length]= 0;               // Ensure we have end 0 for snprintf

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
  if (field_name)
    cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                       ER(ER_TRUNCATED_WRONG_VALUE_FOR_FIELD),
                       type_str, str.c_ptr(), field_name,
                       (ulong) thd->warning_info->current_row_for_warning());
  else
  {
    if (time_type > MYSQL_TIMESTAMP_ERROR)
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER(ER_TRUNCATED_WRONG_VALUE),
                         type_str, str.c_ptr());
    else
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER(ER_WRONG_VALUE), type_str, str.c_ptr());
  }
  push_warning(thd, level,
               ER_TRUNCATED_WRONG_VALUE, warn_buff);
}

/* Daynumber from year 0 to 9999-12-31 */
#define MAX_DAY_NUMBER 3652424L

bool date_add_interval(MYSQL_TIME *ltime, interval_type int_type, INTERVAL interval)
{
  long period, sign;

  ltime->neg= 0;

  sign= (interval.neg ? -1 : 1);

  switch (int_type) {
  case INTERVAL_SECOND:
  case INTERVAL_SECOND_MICROSECOND:
  case INTERVAL_MICROSECOND:
  case INTERVAL_MINUTE:
  case INTERVAL_HOUR:
  case INTERVAL_MINUTE_MICROSECOND:
  case INTERVAL_MINUTE_SECOND:
  case INTERVAL_HOUR_MICROSECOND:
  case INTERVAL_HOUR_SECOND:
  case INTERVAL_HOUR_MINUTE:
  case INTERVAL_DAY_MICROSECOND:
  case INTERVAL_DAY_SECOND:
  case INTERVAL_DAY_MINUTE:
  case INTERVAL_DAY_HOUR:
  {
    longlong sec, days, daynr, microseconds, extra_sec;
    ltime->time_type= MYSQL_TIMESTAMP_DATETIME; // Return full date
    microseconds= ltime->second_part + sign*interval.second_part;
    extra_sec= microseconds/1000000L;
    microseconds= microseconds%1000000L;

    sec=((ltime->day-1)*3600*24L+ltime->hour*3600+ltime->minute*60+
	 ltime->second +
	 sign* (longlong) (interval.day*3600*24L +
                           interval.hour*LL(3600)+interval.minute*LL(60)+
                           interval.second))+ extra_sec;
    if (microseconds < 0)
    {
      microseconds+= LL(1000000);
      sec--;
    }
    days= sec/(3600*LL(24));
    sec-= days*3600*LL(24);
    if (sec < 0)
    {
      days--;
      sec+= 3600*LL(24);
    }
    ltime->second_part= (uint) microseconds;
    ltime->second= (uint) (sec % 60);
    ltime->minute= (uint) (sec/60 % 60);
    ltime->hour=   (uint) (sec/3600);
    daynr= calc_daynr(ltime->year,ltime->month,1) + days;
    /* Day number from year 0 to 9999-12-31 */
    if ((ulonglong) daynr > MAX_DAY_NUMBER)
      goto invalid_date;
    get_date_from_daynr((long) daynr, &ltime->year, &ltime->month,
                        &ltime->day);
    break;
  }
  case INTERVAL_DAY:
  case INTERVAL_WEEK:
    period= (calc_daynr(ltime->year,ltime->month,ltime->day) +
             sign * (long) interval.day);
    /* Daynumber from year 0 to 9999-12-31 */
    if ((ulong) period > MAX_DAY_NUMBER)
      goto invalid_date;
    get_date_from_daynr((long) period,&ltime->year,&ltime->month,&ltime->day);
    break;
  case INTERVAL_YEAR:
    ltime->year+= sign * (long) interval.year;
    if ((ulong) ltime->year >= 10000L)
      goto invalid_date;
    if (ltime->month == 2 && ltime->day == 29 &&
	calc_days_in_year(ltime->year) != 366)
      ltime->day=28;				// Was leap-year
    break;
  case INTERVAL_YEAR_MONTH:
  case INTERVAL_QUARTER:
  case INTERVAL_MONTH:
    period= (ltime->year*12 + sign * (long) interval.year*12 +
	     ltime->month-1 + sign * (long) interval.month);
    if ((ulong) period >= 120000L)
      goto invalid_date;
    ltime->year= (uint) (period / 12);
    ltime->month= (uint) (period % 12L)+1;
    /* Adjust day if the new month doesn't have enough days */
    if (ltime->day > days_in_month[ltime->month-1])
    {
      ltime->day = days_in_month[ltime->month-1];
      if (ltime->month == 2 && calc_days_in_year(ltime->year) == 366)
	ltime->day++;				// Leap-year
    }
    break;
  default:
    goto null_date;
  }

  return 0;					// Ok

invalid_date:
  push_warning_printf(current_thd, MYSQL_ERROR::WARN_LEVEL_WARN,
                      ER_DATETIME_FUNCTION_OVERFLOW,
                      ER(ER_DATETIME_FUNCTION_OVERFLOW),
                      "datetime");
null_date:
  return 1;
}


/*
  Calculate difference between two datetime values as seconds + microseconds.

  SYNOPSIS
    calc_time_diff()
      l_time1         - TIME/DATE/DATETIME value
      l_time2         - TIME/DATE/DATETIME value
      l_sign          - 1 absolute values are substracted,
                        -1 absolute values are added.
      seconds_out     - Out parameter where difference between
                        l_time1 and l_time2 in seconds is stored.
      microseconds_out- Out parameter where microsecond part of difference
                        between l_time1 and l_time2 is stored.

  NOTE
    This function calculates difference between l_time1 and l_time2 absolute
    values. So one should set l_sign and correct result if he want to take
    signs into account (i.e. for MYSQL_TIME values).

  RETURN VALUES
    Returns sign of difference.
    1 means negative result
    0 means positive result

*/

bool
calc_time_diff(MYSQL_TIME *l_time1, MYSQL_TIME *l_time2, int l_sign, longlong *seconds_out,
               long *microseconds_out)
{
  long days;
  bool neg;
  longlong microseconds;

  /*
    We suppose that if first argument is MYSQL_TIMESTAMP_TIME
    the second argument should be TIMESTAMP_TIME also.
    We should check it before calc_time_diff call.
  */
  if (l_time1->time_type == MYSQL_TIMESTAMP_TIME)  // Time value
    days= (long)l_time1->day - l_sign * (long)l_time2->day;
  else
  {
    days= calc_daynr((uint) l_time1->year,
		     (uint) l_time1->month,
		     (uint) l_time1->day);
    if (l_time2->time_type == MYSQL_TIMESTAMP_TIME)
      days-= l_sign * (long)l_time2->day;
    else
      days-= l_sign*calc_daynr((uint) l_time2->year,
			       (uint) l_time2->month,
			       (uint) l_time2->day);
  }

  microseconds= ((longlong)days*LL(86400) +
                 (longlong)(l_time1->hour*3600L +
                            l_time1->minute*60L +
                            l_time1->second) -
                 l_sign*(longlong)(l_time2->hour*3600L +
                                   l_time2->minute*60L +
                                   l_time2->second)) * LL(1000000) +
                (longlong)l_time1->second_part -
                l_sign*(longlong)l_time2->second_part;

  neg= 0;
  if (microseconds < 0)
  {
    microseconds= -microseconds;
    neg= 1;
  }
  *seconds_out= microseconds/1000000L;
  *microseconds_out= (long) (microseconds%1000000L);
  return neg;
}


/*
  Compares 2 MYSQL_TIME structures

  SYNOPSIS
    my_time_compare()

      a - first time
      b - second time

  RETURN VALUE
   -1   - a < b
    0   - a == b
    1   - a > b

*/

int my_time_compare(MYSQL_TIME *a, MYSQL_TIME *b)
{
  ulonglong a_t= TIME_to_ulonglong_datetime(a);
  ulonglong b_t= TIME_to_ulonglong_datetime(b);

  if (a_t < b_t)
    return -1;
  if (a_t > b_t)
    return 1;

  if (a->second_part < b->second_part)
    return -1;
  if (a->second_part > b->second_part)
    return 1;

  return 0;
}

#endif
