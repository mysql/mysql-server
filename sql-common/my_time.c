/* Copyright (C) 2004-2006 MySQL AB

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

#include <my_time.h>
#include <m_string.h>
#include <m_ctype.h>
/* Windows version of localtime_r() is declared in my_ptrhead.h */
#include <my_pthread.h>

ulonglong log_10_int[20]=
{
  1, 10, 100, 1000, 10000UL, 100000UL, 1000000UL, 10000000UL,
  ULL(100000000), ULL(1000000000), ULL(10000000000), ULL(100000000000),
  ULL(1000000000000), ULL(10000000000000), ULL(100000000000000),
  ULL(1000000000000000), ULL(10000000000000000), ULL(100000000000000000),
  ULL(1000000000000000000), ULL(10000000000000000000)
};


/* Position for YYYY-DD-MM HH-MM-DD.FFFFFF AM in default format */

static uchar internal_format_positions[]=
{0, 1, 2, 3, 4, 5, 6, (uchar) 255};

static char time_separator=':';

static ulong const days_at_timestart=719528;	/* daynr at 1970.01.01 */
uchar days_in_month[]= {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0};

/*
  Offset of system time zone from UTC in seconds used to speed up 
  work of my_system_gmt_sec() function.
*/
static long my_time_zone=0;


/* Calc days in one year. works with 0 <= year <= 99 */

uint calc_days_in_year(uint year)
{
  return ((year & 3) == 0 && (year%100 || (year%400 == 0 && year)) ?
          366 : 365);
}

/**
  @brief Check datetime value for validity according to flags.

  @param[in]  ltime          Date to check.
  @param[in]  not_zero_date  ltime is not the zero date
  @param[in]  flags          flags to check
                             (see str_to_datetime() flags in my_time.h)
  @param[out] was_cut        set to 2 if value was invalid according to flags.
                             (Feb 29 in non-leap etc.)  This remains unchanged
                             if value is not invalid.

  @details Here we assume that year and month is ok!
    If month is 0 we allow any date. (This only happens if we allow zero
    date parts in str_to_datetime())
    Disallow dates with zero year and non-zero month and/or day.

  @return
    0  OK
    1  error
*/

my_bool check_date(const MYSQL_TIME *ltime, my_bool not_zero_date,
                   ulong flags, int *was_cut)
{
  if (not_zero_date)
  {
    if ((((flags & TIME_NO_ZERO_IN_DATE) || !(flags & TIME_FUZZY_DATE)) &&
         (ltime->month == 0 || ltime->day == 0)) ||
        (!(flags & TIME_INVALID_DATES) &&
         ltime->month && ltime->day > days_in_month[ltime->month-1] &&
         (ltime->month != 2 || calc_days_in_year(ltime->year) != 366 ||
          ltime->day != 29)))
    {
      *was_cut= 2;
      return TRUE;
    }
  }
  else if (flags & TIME_NO_ZERO_DATE)
  {
    /*
      We don't set *was_cut here to signal that the problem was a zero date
      and not an invalid date
    */
    return TRUE;
  }
  return FALSE;
}


/*
  Convert a timestamp string to a MYSQL_TIME value.

  SYNOPSIS
    str_to_datetime()
    str                 String to parse
    length              Length of string
    l_time              Date is stored here
    flags               Bitmap of following items
                        TIME_FUZZY_DATE    Set if we should allow partial dates
                        TIME_DATETIME_ONLY Set if we only allow full datetimes.
                        TIME_NO_ZERO_IN_DATE	Don't allow partial dates
                        TIME_NO_ZERO_DATE	Don't allow 0000-00-00 date
                        TIME_INVALID_DATES	Allow 2000-02-31
    was_cut             0	Value OK
			1       If value was cut during conversion
			2	check_date(date,flags) considers date invalid

  DESCRIPTION
    At least the following formats are recogniced (based on number of digits)
    YYMMDD, YYYYMMDD, YYMMDDHHMMSS, YYYYMMDDHHMMSS
    YY-MM-DD, YYYY-MM-DD, YY-MM-DD HH.MM.SS
    YYYYMMDDTHHMMSS  where T is a the character T (ISO8601)
    Also dates where all parts are zero are allowed

    The second part may have an optional .###### fraction part.

  NOTES
   This function should work with a format position vector as long as the
   following things holds:
   - All date are kept together and all time parts are kept together
   - Date and time parts must be separated by blank
   - Second fractions must come after second part and be separated
     by a '.'.  (The second fractions are optional)
   - AM/PM must come after second fractions (or after seconds if no fractions)
   - Year must always been specified.
   - If time is before date, then we will use datetime format only if
     the argument consist of two parts, separated by space.
     Otherwise we will assume the argument is a date.
   - The hour part must be specified in hour-minute-second order.

  RETURN VALUES
    MYSQL_TIMESTAMP_NONE        String wasn't a timestamp, like
                                [DD [HH:[MM:[SS]]]].fraction.
                                l_time is not changed.
    MYSQL_TIMESTAMP_DATE        DATE string (YY MM and DD parts ok)
    MYSQL_TIMESTAMP_DATETIME    Full timestamp
    MYSQL_TIMESTAMP_ERROR       Timestamp with wrong values.
                                All elements in l_time is set to 0
*/

#define MAX_DATE_PARTS 8

enum enum_mysql_timestamp_type
str_to_datetime(const char *str, uint length, MYSQL_TIME *l_time,
                uint flags, int *was_cut)
{
  uint field_length, UNINIT_VAR(year_length), digits, i, number_of_fields;
  uint date[MAX_DATE_PARTS], date_len[MAX_DATE_PARTS];
  uint add_hours= 0, start_loop;
  ulong not_zero_date, allow_space;
  my_bool is_internal_format;
  const char *pos, *UNINIT_VAR(last_field_pos);
  const char *end=str+length;
  const uchar *format_position;
  my_bool found_delimitier= 0, found_space= 0;
  uint frac_pos, frac_len;
  DBUG_ENTER("str_to_datetime");
  DBUG_PRINT("ENTER",("str: %.*s",length,str));

  LINT_INIT(field_length);

  *was_cut= 0;

  /* Skip space at start */
  for (; str != end && my_isspace(&my_charset_latin1, *str) ; str++)
    ;
  if (str == end || ! my_isdigit(&my_charset_latin1, *str))
  {
    *was_cut= 1;
    DBUG_RETURN(MYSQL_TIMESTAMP_NONE);
  }

  is_internal_format= 0;
  /* This has to be changed if want to activate different timestamp formats */
  format_position= internal_format_positions;

  /*
    Calculate number of digits in first part.
    If length= 8 or >= 14 then year is of format YYYY.
    (YYYY-MM-DD,  YYYYMMDD, YYYYYMMDDHHMMSS)
  */
  for (pos=str;
       pos != end && (my_isdigit(&my_charset_latin1,*pos) || *pos == 'T');
       pos++)
    ;

  digits= (uint) (pos-str);
  start_loop= 0;                                /* Start of scan loop */
  date_len[format_position[0]]= 0;              /* Length of year field */
  if (pos == end || *pos == '.')
  {
    /* Found date in internal format (only numbers like YYYYMMDD) */
    year_length= (digits == 4 || digits == 8 || digits >= 14) ? 4 : 2;
    field_length= year_length;
    is_internal_format= 1;
    format_position= internal_format_positions;
  }
  else
  {
    if (format_position[0] >= 3)                /* If year is after HHMMDD */
    {
      /*
        If year is not in first part then we have to determinate if we got
        a date field or a datetime field.
        We do this by checking if there is two numbers separated by
        space in the input.
      */
      while (pos < end && !my_isspace(&my_charset_latin1, *pos))
        pos++;
      while (pos < end && !my_isdigit(&my_charset_latin1, *pos))
        pos++;
      if (pos == end)
      {
        if (flags & TIME_DATETIME_ONLY)
        {
          *was_cut= 1;
          DBUG_RETURN(MYSQL_TIMESTAMP_NONE);   /* Can't be a full datetime */
        }
        /* Date field.  Set hour, minutes and seconds to 0 */
        date[0]= date[1]= date[2]= date[3]= date[4]= 0;
        start_loop= 5;                         /* Start with first date part */
      }
    }

    field_length= format_position[0] == 0 ? 4 : 2;
  }

  /*
    Only allow space in the first "part" of the datetime field and:
    - after days, part seconds
    - before and after AM/PM (handled by code later)

    2003-03-03 20:00:20 AM
    20:00:20.000000 AM 03-03-2000
  */
  i= max((uint) format_position[0], (uint) format_position[1]);
  set_if_bigger(i, (uint) format_position[2]);
  allow_space= ((1 << i) | (1 << format_position[6]));
  allow_space&= (1 | 2 | 4 | 8);

  not_zero_date= 0;
  for (i = start_loop;
       i < MAX_DATE_PARTS-1 && str != end &&
         my_isdigit(&my_charset_latin1,*str);
       i++)
  {
    const char *start= str;
    ulong tmp_value= (uint) (uchar) (*str++ - '0');

    /*
      Internal format means no delimiters; every field has a fixed
      width. Otherwise, we scan until we find a delimiter and discard
      leading zeroes -- except for the microsecond part, where leading
      zeroes are significant, and where we never process more than six
      digits.
    */
    my_bool     scan_until_delim= !is_internal_format &&
                                  ((i != format_position[6]));

    while (str != end && my_isdigit(&my_charset_latin1,str[0]) &&
           (scan_until_delim || --field_length))
    {
      tmp_value=tmp_value*10 + (ulong) (uchar) (*str - '0');
      str++;
    }
    date_len[i]= (uint) (str - start);
    if (tmp_value > 999999)                     /* Impossible date part */
    {
      *was_cut= 1;
      DBUG_RETURN(MYSQL_TIMESTAMP_NONE);
    }
    date[i]=tmp_value;
    not_zero_date|= tmp_value;

    /* Length of next field */
    field_length= format_position[i+1] == 0 ? 4 : 2;

    if ((last_field_pos= str) == end)
    {
      i++;                                      /* Register last found part */
      break;
    }
    /* Allow a 'T' after day to allow CCYYMMDDT type of fields */
    if (i == format_position[2] && *str == 'T')
    {
      str++;                                    /* ISO8601:  CCYYMMDDThhmmss */
      continue;
    }
    if (i == format_position[5])                /* Seconds */
    {
      if (*str == '.')                          /* Followed by part seconds */
      {
        str++;
        field_length= 6;                        /* 6 digits */
      }
      continue;
    }
    while (str != end &&
           (my_ispunct(&my_charset_latin1,*str) ||
            my_isspace(&my_charset_latin1,*str)))
    {
      if (my_isspace(&my_charset_latin1,*str))
      {
        if (!(allow_space & (1 << i)))
        {
          *was_cut= 1;
          DBUG_RETURN(MYSQL_TIMESTAMP_NONE);
        }
        found_space= 1;
      }
      str++;
      found_delimitier= 1;                      /* Should be a 'normal' date */
    }
    /* Check if next position is AM/PM */
    if (i == format_position[6])                /* Seconds, time for AM/PM */
    {
      i++;                                      /* Skip AM/PM part */
      if (format_position[7] != 255)            /* If using AM/PM */
      {
        if (str+2 <= end && (str[1] == 'M' || str[1] == 'm'))
        {
          if (str[0] == 'p' || str[0] == 'P')
            add_hours= 12;
          else if (str[0] != 'a' || str[0] != 'A')
            continue;                           /* Not AM/PM */
          str+= 2;                              /* Skip AM/PM */
          /* Skip space after AM/PM */
          while (str != end && my_isspace(&my_charset_latin1,*str))
            str++;
        }
      }
    }
    last_field_pos= str;
  }
  if (found_delimitier && !found_space && (flags & TIME_DATETIME_ONLY))
  {
    *was_cut= 1;
    DBUG_RETURN(MYSQL_TIMESTAMP_NONE);          /* Can't be a datetime */
  }

  str= last_field_pos;

  number_of_fields= i - start_loop;
  while (i < MAX_DATE_PARTS)
  {
    date_len[i]= 0;
    date[i++]= 0;
  }

  if (!is_internal_format)
  {
    year_length= date_len[(uint) format_position[0]];
    if (!year_length)                           /* Year must be specified */
    {
      *was_cut= 1;
      DBUG_RETURN(MYSQL_TIMESTAMP_NONE);
    }

    l_time->year=               date[(uint) format_position[0]];
    l_time->month=              date[(uint) format_position[1]];
    l_time->day=                date[(uint) format_position[2]];
    l_time->hour=               date[(uint) format_position[3]];
    l_time->minute=             date[(uint) format_position[4]];
    l_time->second=             date[(uint) format_position[5]];

    frac_pos= (uint) format_position[6];
    frac_len= date_len[frac_pos];
    if (frac_len < 6)
      date[frac_pos]*= (uint) log_10_int[6 - frac_len];
    l_time->second_part= date[frac_pos];

    if (format_position[7] != (uchar) 255)
    {
      if (l_time->hour > 12)
      {
        *was_cut= 1;
        goto err;
      }
      l_time->hour= l_time->hour%12 + add_hours;
    }
  }
  else
  {
    l_time->year=       date[0];
    l_time->month=      date[1];
    l_time->day=        date[2];
    l_time->hour=       date[3];
    l_time->minute=     date[4];
    l_time->second=     date[5];
    if (date_len[6] < 6)
      date[6]*= (uint) log_10_int[6 - date_len[6]];
    l_time->second_part=date[6];
  }
  l_time->neg= 0;

  if (year_length == 2 && not_zero_date)
    l_time->year+= (l_time->year < YY_PART_YEAR ? 2000 : 1900);

  if (number_of_fields < 3 ||
      l_time->year > 9999 || l_time->month > 12 ||
      l_time->day > 31 || l_time->hour > 23 ||
      l_time->minute > 59 || l_time->second > 59)
  {
    /* Only give warning for a zero date if there is some garbage after */
    if (!not_zero_date)                         /* If zero date */
    {
      for (; str != end ; str++)
      {
        if (!my_isspace(&my_charset_latin1, *str))
        {
          not_zero_date= 1;                     /* Give warning */
          break;
        }
      }
    }
    *was_cut= test(not_zero_date);
    goto err;
  }

  if (check_date(l_time, not_zero_date != 0, flags, was_cut))
    goto err;

  l_time->time_type= (number_of_fields <= 3 ?
                      MYSQL_TIMESTAMP_DATE : MYSQL_TIMESTAMP_DATETIME);

  for (; str != end ; str++)
  {
    if (!my_isspace(&my_charset_latin1,*str))
    {
      *was_cut= 1;
      break;
    }
  }

  DBUG_RETURN(l_time->time_type);

err:
  bzero((char*) l_time, sizeof(*l_time));
  DBUG_RETURN(MYSQL_TIMESTAMP_ERROR);
}


/*
 Convert a time string to a MYSQL_TIME struct.

  SYNOPSIS
   str_to_time()
   str                  A string in full TIMESTAMP format or
                        [-] DAYS [H]H:MM:SS, [H]H:MM:SS, [M]M:SS, [H]HMMSS,
                        [M]MSS or [S]S
                        There may be an optional [.second_part] after seconds
   length               Length of str
   l_time               Store result here
   warning              Set MYSQL_TIME_WARN_TRUNCATED flag if the input string
                        was cut during conversion, and/or
                        MYSQL_TIME_WARN_OUT_OF_RANGE flag, if the value is
                        out of range.

   NOTES
     Because of the extra days argument, this function can only
     work with times where the time arguments are in the above order.

   RETURN
     0  ok
     1  error
*/

my_bool str_to_time(const char *str, uint length, MYSQL_TIME *l_time,
                    int *warning)
{
  ulong date[5];
  ulonglong value;
  const char *end=str+length, *end_of_days;
  my_bool found_days,found_hours;
  uint state;

  l_time->neg=0;
  *warning= 0;
  for (; str != end && my_isspace(&my_charset_latin1,*str) ; str++)
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
  {                                             /* Probably full timestamp */
    int was_cut;
    enum enum_mysql_timestamp_type
      res= str_to_datetime(str, length, l_time,
                           (TIME_FUZZY_DATE | TIME_DATETIME_ONLY), &was_cut);
    if ((int) res >= (int) MYSQL_TIMESTAMP_ERROR)
    {
      if (was_cut)
        *warning|= MYSQL_TIME_WARN_TRUNCATED;
      return res == MYSQL_TIMESTAMP_ERROR;
    }
  }

  /* Not a timestamp. Try to get this as a DAYS_TO_SECOND string */
  for (value=0; str != end && my_isdigit(&my_charset_latin1,*str) ; str++)
    value=value*10L + (long) (*str - '0');

  /* Skip all space after 'days' */
  end_of_days= str;
  for (; str != end && my_isspace(&my_charset_latin1, str[0]) ; str++)
    ;

  LINT_INIT(state);
  found_days=found_hours=0;
  if ((uint) (end-str) > 1 && str != end_of_days &&
      my_isdigit(&my_charset_latin1, *str))
  {                                             /* Found days part */
    date[0]= (ulong) value;
    state= 1;                                   /* Assume next is hours */
    found_days= 1;
  }
  else if ((end-str) > 1 &&  *str == time_separator &&
           my_isdigit(&my_charset_latin1, str[1]))
  {
    date[0]= 0;                                 /* Assume we found hours */
    date[1]= (ulong) value;
    state=2;
    found_hours=1;
    str++;                                      /* skip ':' */
  }
  else
  {
    /* String given as one number; assume HHMMSS format */
    date[0]= 0;
    date[1]= (ulong) (value/10000);
    date[2]= (ulong) (value/100 % 100);
    date[3]= (ulong) (value % 100);
    state=4;
    goto fractional;
  }

  /* Read hours, minutes and seconds */
  for (;;)
  {
    for (value=0; str != end && my_isdigit(&my_charset_latin1,*str) ; str++)
      value=value*10L + (long) (*str - '0');
    date[state++]= (ulong) value;
    if (state == 4 || (end-str) < 2 || *str != time_separator ||
        !my_isdigit(&my_charset_latin1,str[1]))
      break;
    str++;                                      /* Skip time_separator (':') */
  }

  if (state != 4)
  {                                             /* Not HH:MM:SS */
    /* Fix the date to assume that seconds was given */
    if (!found_hours && !found_days)
    {
      bmove_upp((uchar*) (date+4), (uchar*) (date+state),
                sizeof(long)*(state-1));
      bzero((uchar*) date, sizeof(long)*(4-state));
    }
    else
      bzero((uchar*) (date+state), sizeof(long)*(4-state));
  }

fractional:
  /* Get fractional second part */
  if ((end-str) >= 2 && *str == '.' && my_isdigit(&my_charset_latin1,str[1]))
  {
    int field_length= 5;
    str++; value=(uint) (uchar) (*str - '0');
    while (++str != end && my_isdigit(&my_charset_latin1, *str))
    {
      if (field_length-- > 0)
        value= value*10 + (uint) (uchar) (*str - '0');
    }
    if (field_length > 0)
      value*= (long) log_10_int[field_length];
    else if (field_length < 0)
      *warning|= MYSQL_TIME_WARN_TRUNCATED;
    date[4]= (ulong) value;
  }
  else
    date[4]=0;
    
  /* Check for exponent part: E<gigit> | E<sign><digit> */
  /* (may occur as result of %g formatting of time value) */
  if ((end - str) > 1 &&
      (*str == 'e' || *str == 'E') &&
      (my_isdigit(&my_charset_latin1, str[1]) ||
       ((str[1] == '-' || str[1] == '+') &&
        (end - str) > 2 &&
        my_isdigit(&my_charset_latin1, str[2]))))
    return 1;

  if (internal_format_positions[7] != 255)
  {
    /* Read a possible AM/PM */
    while (str != end && my_isspace(&my_charset_latin1, *str))
      str++;
    if (str+2 <= end && (str[1] == 'M' || str[1] == 'm'))
    {
      if (str[0] == 'p' || str[0] == 'P')
      {
        str+= 2;
        date[1]= date[1]%12 + 12;
      }
      else if (str[0] == 'a' || str[0] == 'A')
        str+=2;
    }
  }

  /* Integer overflow checks */
  if (date[0] > UINT_MAX || date[1] > UINT_MAX ||
      date[2] > UINT_MAX || date[3] > UINT_MAX ||
      date[4] > UINT_MAX)
    return 1;
  
  l_time->year=         0;                      /* For protocol::store_time */
  l_time->month=        0;
  l_time->day=          date[0];
  l_time->hour=         date[1];
  l_time->minute=       date[2];
  l_time->second=       date[3];
  l_time->second_part=  date[4];
  l_time->time_type= MYSQL_TIMESTAMP_TIME;

  /* Check if the value is valid and fits into MYSQL_TIME range */
  if (check_time_range(l_time, warning))
    return 1;
  
  /* Check if there is garbage at end of the MYSQL_TIME specification */
  if (str != end)
  {
    do
    {
      if (!my_isspace(&my_charset_latin1,*str))
      {
        *warning|= MYSQL_TIME_WARN_TRUNCATED;
        break;
      }
    } while (++str != end);
  }
  return 0;
}


/*
  Check 'time' value to lie in the MYSQL_TIME range

  SYNOPSIS:
    check_time_range()
    time     pointer to MYSQL_TIME value
    warning  set MYSQL_TIME_WARN_OUT_OF_RANGE flag if the value is out of range

  DESCRIPTION
  If the time value lies outside of the range [-838:59:59, 838:59:59],
  set it to the closest endpoint of the range and set
  MYSQL_TIME_WARN_OUT_OF_RANGE flag in the 'warning' variable.

  RETURN
    0        time value is valid, but was possibly truncated
    1        time value is invalid
*/

int check_time_range(struct st_mysql_time *my_time, int *warning) 
{
  longlong hour;

  if (my_time->minute >= 60 || my_time->second >= 60)
    return 1;

  hour= my_time->hour + (24*my_time->day);
  if (hour <= TIME_MAX_HOUR &&
      (hour != TIME_MAX_HOUR || my_time->minute != TIME_MAX_MINUTE ||
       my_time->second != TIME_MAX_SECOND || !my_time->second_part))
    return 0;

  my_time->day= 0;
  my_time->hour= TIME_MAX_HOUR;
  my_time->minute= TIME_MAX_MINUTE;
  my_time->second= TIME_MAX_SECOND;
  my_time->second_part= 0;
  *warning|= MYSQL_TIME_WARN_OUT_OF_RANGE;
  return 0;
}


/*
  Prepare offset of system time zone from UTC for my_system_gmt_sec() func.

  SYNOPSIS
    my_init_time()
*/
void my_init_time(void)
{
  time_t seconds;
  struct tm *l_time,tm_tmp;
  MYSQL_TIME my_time;
  my_bool not_used;

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
  my_system_gmt_sec(&my_time, &my_time_zone, &not_used); /* Init my_time_zone */
}


/*
  Handle 2 digit year conversions

  SYNOPSIS
  year_2000_handling()
  year     2 digit year

  RETURN
    Year between 1970-2069
*/

uint year_2000_handling(uint year)
{
  if ((year=year+1900) < 1900+YY_PART_YEAR)
    year+=100;
  return year;
}


/*
  Calculate nr of day since year 0 in new date-system (from 1615)

  SYNOPSIS
    calc_daynr()
    year		 Year (exact 4 digit year, no year conversions)
    month		 Month
    day			 Day

  NOTES: 0000-00-00 is a valid date, and will return 0

  RETURN
    Days since 0000-00-00
*/

long calc_daynr(uint year,uint month,uint day)
{
  long delsum;
  int temp;
  int y= year;                                  /* may be < 0 temporarily */
  DBUG_ENTER("calc_daynr");

  if (y == 0 && month == 0 && day == 0)
    DBUG_RETURN(0);				/* Skip errors */
  /* Cast to int to be able to handle month == 0 */
  delsum= (long) (365 * y + 31 *((int) month - 1) + (int) day);
  if (month <= 2)
      y--;
  else
    delsum-= (long) ((int) month * 4 + 23) / 10;
  temp=(int) ((y/100+1)*3)/4;
  DBUG_PRINT("exit",("year: %d  month: %d  day: %d -> daynr: %ld",
		     y+(month <= 2),month,day,delsum+y/4-temp));
  DBUG_RETURN(delsum+(int) y/4-temp);
} /* calc_daynr */


/*
  Convert time in MYSQL_TIME representation in system time zone to its
  my_time_t form (number of seconds in UTC since begginning of Unix Epoch).

  SYNOPSIS
    my_system_gmt_sec()
      t               - time value to be converted
      my_timezone     - pointer to long where offset of system time zone
                        from UTC will be stored for caching
      in_dst_time_gap - set to true if time falls into spring time-gap

  NOTES
    The idea is to cache the time zone offset from UTC (including daylight 
    saving time) for the next call to make things faster. But currently we 
    just calculate this offset during startup (by calling my_init_time() 
    function) and use it all the time.
    Time value provided should be legal time value (e.g. '2003-01-01 25:00:00'
    is not allowed).

  RETURN VALUE
    Time in UTC seconds since Unix Epoch representation.
*/
my_time_t
my_system_gmt_sec(const MYSQL_TIME *t_src, long *my_timezone,
                  my_bool *in_dst_time_gap)
{
  uint loop;
  time_t tmp= 0;
  int shift= 0;
  MYSQL_TIME tmp_time;
  MYSQL_TIME *t= &tmp_time;
  struct tm *l_time,tm_tmp;
  long diff, current_timezone;

  /*
    Use temp variable to avoid trashing input data, which could happen in
    case of shift required for boundary dates processing.
  */
  memcpy(&tmp_time, t_src, sizeof(MYSQL_TIME));

  if (!validate_timestamp_range(t))
    return 0;

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

    Note2: For the dates, which have time_t representation close to
    MAX_INT32 (efficient time_t limit for supported platforms), we should
    do a small trick to avoid overflow. That is, convert the date, which is
    two days earlier, and then add these days to the final value.

    The same trick is done for the values close to 0 in time_t
    representation for platfroms with unsigned time_t (QNX).

    To be more verbose, here is a sample (extracted from the code below):
    (calc_daynr(2038, 1, 19) - (long) days_at_timestart)*86400L + 4*3600L
    would return -2147480896 because of the long type overflow. In result
    we would get 1901 year in localtime_r(), which is an obvious error.

    Alike problem raises with the dates close to Epoch. E.g.
    (calc_daynr(1969, 12, 31) - (long) days_at_timestart)*86400L + 23*3600L
    will give -3600.

    On some platforms, (E.g. on QNX) time_t is unsigned and localtime(-3600)
    wil give us a date around 2106 year. Which is no good.

    Theoreticaly, there could be problems with the latter conversion:
    there are at least two timezones, which had time switches near 1 Jan
    of 1970 (because of political reasons). These are America/Hermosillo and
    America/Mazatlan time zones. They changed their offset on
    1970-01-01 08:00:00 UTC from UTC-8 to UTC-7. For these zones
    the code below will give incorrect results for dates close to
    1970-01-01, in the case OS takes into account these historical switches.
    Luckily, it seems that we support only one platform with unsigned
    time_t. It's QNX. And QNX does not support historical timezone data at all.
    E.g. there are no /usr/share/zoneinfo/ files or any other mean to supply
    historical information for localtime_r() etc. That is, the problem is not
    relevant to QNX.

    We are safe with shifts close to MAX_INT32, as there are no known
    time switches on Jan 2038 yet :)
  */
  if ((t->year == TIMESTAMP_MAX_YEAR) && (t->month == 1) && (t->day > 4))
  {
    /*
      Below we will pass (uint) (t->day - shift) to calc_daynr.
      As we don't want to get an overflow here, we will shift
      only safe dates. That's why we have (t->day > 4) above.
    */
    t->day-= 2;
    shift= 2;
  }
#ifdef TIME_T_UNSIGNED
  else
  {
    /*
      We can get 0 in time_t representaion only on 1969, 31 of Dec or on
      1970, 1 of Jan. For both dates we use shift, which is added
      to t->day in order to step out a bit from the border.
      This is required for platforms, where time_t is unsigned.
      As far as I know, among the platforms we support it's only QNX.
      Note: the order of below if-statements is significant.
    */

    if ((t->year == TIMESTAMP_MIN_YEAR + 1) && (t->month == 1)
        && (t->day <= 10))
    {
      t->day+= 2;
      shift= -2;
    }

    if ((t->year == TIMESTAMP_MIN_YEAR) && (t->month == 12)
        && (t->day == 31))
    {
      t->year++;
      t->month= 1;
      t->day= 2;
      shift= -2;
    }
  }
#endif

  tmp= (time_t) (((calc_daynr((uint) t->year, (uint) t->month, (uint) t->day) -
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
      days= 1;					/* Month has wrapped */
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour)) +
          (long) (60*((int) t->minute - (int) l_time->tm_min)) +
          (long) ((int) t->second - (int) l_time->tm_sec));
    current_timezone+= diff+3600;		/* Compensate for -3600 above */
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
      days=1;					/* Month has wrapped */
    else if (days > 1)
      days= -1;
    diff=(3600L*(long) (days*24+((int) t->hour - (int) l_time->tm_hour))+
	  (long) (60*((int) t->minute - (int) l_time->tm_min)) +
          (long) ((int) t->second - (int) l_time->tm_sec));
    if (diff == 3600)
      tmp+=3600 - t->minute*60 - t->second;	/* Move to next hour */
    else if (diff == -3600)
      tmp-=t->minute*60 + t->second;		/* Move to previous hour */

    *in_dst_time_gap= 1;
  }
  *my_timezone= current_timezone;


  /* shift back, if we were dealing with boundary dates */
  tmp+= shift*86400L;

  /*
    This is possible for dates, which slightly exceed boundaries.
    Conversion will pass ok for them, but we don't allow them.
    First check will pass for platforms with signed time_t.
    instruction above (tmp+= shift*86400L) could exceed
    MAX_INT32 (== TIMESTAMP_MAX_VALUE) and overflow will happen.
    So, tmp < TIMESTAMP_MIN_VALUE will be triggered. On platfroms
    with unsigned time_t tmp+= shift*86400L might result in a number,
    larger then TIMESTAMP_MAX_VALUE, so another check will work.
  */
  if ((tmp < TIMESTAMP_MIN_VALUE) || (tmp > TIMESTAMP_MAX_VALUE))
    tmp= 0;

  return (my_time_t) tmp;
} /* my_system_gmt_sec */


/* Set MYSQL_TIME structure to 0000-00-00 00:00:00.000000 */

void set_zero_time(MYSQL_TIME *tm, enum enum_mysql_timestamp_type time_type)
{
  bzero((void*) tm, sizeof(*tm));
  tm->time_type= time_type;
}


/*
  Functions to convert time/date/datetime value to a string,
  using default format.
  This functions don't check that given MYSQL_TIME structure members are
  in valid range. If they are not, return value won't reflect any
  valid date either. Additionally, make_time doesn't take into
  account time->day member: it's assumed that days have been converted
  to hours already.

  RETURN
    number of characters written to 'to'
*/

int my_time_to_str(const MYSQL_TIME *l_time, char *to)
{
  uint extra_hours= 0;
  return sprintf(to, "%s%02u:%02u:%02u", (l_time->neg ? "-" : ""),
                 extra_hours+ l_time->hour, l_time->minute, l_time->second);
}

int my_date_to_str(const MYSQL_TIME *l_time, char *to)
{
  return sprintf(to, "%04u-%02u-%02u",
                 l_time->year, l_time->month, l_time->day);
}

int my_datetime_to_str(const MYSQL_TIME *l_time, char *to)
{
  return sprintf(to, "%04u-%02u-%02u %02u:%02u:%02u",
                 l_time->year, l_time->month, l_time->day,
                 l_time->hour, l_time->minute, l_time->second);
}


/*
  Convert struct DATE/TIME/DATETIME value to string using built-in
  MySQL time conversion formats.

  SYNOPSIS
    my_TIME_to_string()

  NOTE
    The string must have at least MAX_DATE_STRING_REP_LENGTH bytes reserved.
*/

int my_TIME_to_str(const MYSQL_TIME *l_time, char *to)
{
  switch (l_time->time_type) {
  case MYSQL_TIMESTAMP_DATETIME:
    return my_datetime_to_str(l_time, to);
  case MYSQL_TIMESTAMP_DATE:
    return my_date_to_str(l_time, to);
  case MYSQL_TIMESTAMP_TIME:
    return my_time_to_str(l_time, to);
  case MYSQL_TIMESTAMP_NONE:
  case MYSQL_TIMESTAMP_ERROR:
    to[0]='\0';
    return 0;
  default:
    DBUG_ASSERT(0);
    return 0;
  }
}


/*
  Convert datetime value specified as number to broken-down TIME
  representation and form value of DATETIME type as side-effect.

  SYNOPSIS
    number_to_datetime()
      nr         - datetime value as number
      time_res   - pointer for structure for broken-down representation
      flags      - flags to use in validating date, as in str_to_datetime()
      was_cut    0      Value ok
                 1      If value was cut during conversion
                 2      check_date(date,flags) considers date invalid

  DESCRIPTION
    Convert a datetime value of formats YYMMDD, YYYYMMDD, YYMMDDHHMSS,
    YYYYMMDDHHMMSS to broken-down MYSQL_TIME representation. Return value in
    YYYYMMDDHHMMSS format as side-effect.

    This function also checks if datetime value fits in DATETIME range.

  RETURN VALUE
    -1              Timestamp with wrong values
    anything else   DATETIME as integer in YYYYMMDDHHMMSS format
    Datetime value in YYYYMMDDHHMMSS format.
*/

longlong number_to_datetime(longlong nr, MYSQL_TIME *time_res,
                            uint flags, int *was_cut)
{
  long part1,part2;

  *was_cut= 0;
  bzero((char*) time_res, sizeof(*time_res));
  time_res->time_type=MYSQL_TIMESTAMP_DATE;

  if (nr == LL(0) || nr >= LL(10000101000000))
  {
    time_res->time_type=MYSQL_TIMESTAMP_DATETIME;
    goto ok;
  }
  if (nr < 101)
    goto err;
  if (nr <= (YY_PART_YEAR-1)*10000L+1231L)
  {
    nr= (nr+20000000L)*1000000L;                 /* YYMMDD, year: 2000-2069 */
    goto ok;
  }
  if (nr < (YY_PART_YEAR)*10000L+101L)
    goto err;
  if (nr <= 991231L)
  {
    nr= (nr+19000000L)*1000000L;                 /* YYMMDD, year: 1970-1999 */
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

  time_res->time_type=MYSQL_TIMESTAMP_DATETIME;

  if (nr <= (YY_PART_YEAR-1)*LL(10000000000)+LL(1231235959))
  {
    nr= nr+LL(20000000000000);                   /* YYMMDDHHMMSS, 2000-2069 */
    goto ok;
  }
  if (nr <  YY_PART_YEAR*LL(10000000000)+ LL(101000000))
    goto err;
  if (nr <= LL(991231235959))
    nr= nr+LL(19000000000000);		/* YYMMDDHHMMSS, 1970-1999 */

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
      !check_date(time_res, (nr != 0), flags, was_cut))
    return nr;

  /* Don't want to have was_cut get set if NO_ZERO_DATE was violated. */
  if (!nr && (flags & TIME_NO_ZERO_DATE))
    return LL(-1);

 err:
  *was_cut= 1;
  return LL(-1);
}


/* Convert time value to integer in YYYYMMDDHHMMSS format */

ulonglong TIME_to_ulonglong_datetime(const MYSQL_TIME *my_time)
{
  return ((ulonglong) (my_time->year * 10000UL +
                       my_time->month * 100UL +
                       my_time->day) * ULL(1000000) +
          (ulonglong) (my_time->hour * 10000UL +
                       my_time->minute * 100UL +
                       my_time->second));
}


/* Convert MYSQL_TIME value to integer in YYYYMMDD format */

ulonglong TIME_to_ulonglong_date(const MYSQL_TIME *my_time)
{
  return (ulonglong) (my_time->year * 10000UL + my_time->month * 100UL +
                      my_time->day);
}


/*
  Convert MYSQL_TIME value to integer in HHMMSS format.
  This function doesn't take into account time->day member:
  it's assumed that days have been converted to hours already.
*/

ulonglong TIME_to_ulonglong_time(const MYSQL_TIME *my_time)
{
  return (ulonglong) (my_time->hour * 10000UL +
                      my_time->minute * 100UL +
                      my_time->second);
}


/*
  Convert struct MYSQL_TIME (date and time split into year/month/day/hour/...
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
    This function doesn't check that given MYSQL_TIME structure members are
    in valid range. If they are not, return value won't reflect any
    valid date either.
*/

ulonglong TIME_to_ulonglong(const MYSQL_TIME *my_time)
{
  switch (my_time->time_type) {
  case MYSQL_TIMESTAMP_DATETIME:
    return TIME_to_ulonglong_datetime(my_time);
  case MYSQL_TIMESTAMP_DATE:
    return TIME_to_ulonglong_date(my_time);
  case MYSQL_TIMESTAMP_TIME:
    return TIME_to_ulonglong_time(my_time);
  case MYSQL_TIMESTAMP_NONE:
  case MYSQL_TIMESTAMP_ERROR:
    return ULL(0);
  default:
    DBUG_ASSERT(0);
  }
  return 0;
}

