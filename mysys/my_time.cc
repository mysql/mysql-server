/* Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 Without limiting anything contained in the foregoing, this file,
 which is part of C Driver for MySQL (Connector/C), is also subject to the
 Universal FOSS Exception, version 1.0, a copy of which can be found at
 http://oss.oracle.com/licenses/universal-foss-exception.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_time.h"

#include <stdio.h>
#include <time.h>

#include "binary_log_types.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_dbug.h"
#include "my_systime.h"
#include "myisampack.h"

ulonglong log_10_int[20] = {1,
                            10,
                            100,
                            1000,
                            10000UL,
                            100000UL,
                            1000000UL,
                            10000000UL,
                            100000000ULL,
                            1000000000ULL,
                            10000000000ULL,
                            100000000000ULL,
                            1000000000000ULL,
                            10000000000000ULL,
                            100000000000000ULL,
                            1000000000000000ULL,
                            10000000000000000ULL,
                            100000000000000000ULL,
                            1000000000000000000ULL,
                            10000000000000000000ULL};

const char my_zero_datetime6[] = "0000-00-00 00:00:00.000000";

/* Position for YYYY-DD-MM HH-MM-DD.FFFFFF AM in default format */

static uchar internal_format_positions[] = {0, 1, 2, 3, 4, 5, 6, (uchar)255};

static char time_separator = ':';

static ulong const days_at_timestart = 719528; /* daynr at 1970.01.01 */
uchar days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31, 0};

/*
  Offset of system time zone from UTC in seconds used to speed up
  work of my_system_gmt_sec() function.
*/
static long my_time_zone = 0;

/* Calc days in one year. works with 0 <= year <= 99 */

uint calc_days_in_year(uint year) {
  return ((year & 3) == 0 && (year % 100 || (year % 400 == 0 && year)) ? 366
                                                                       : 365);
}

/**
   Set MYSQL_TIME structure to 0000-00-00 00:00:00.000000
   @param [out] tm    The value to set.
   @param time_type  Timestasmp type
*/
void set_zero_time(MYSQL_TIME *tm, enum enum_mysql_timestamp_type time_type) {
  memset(tm, 0, sizeof(*tm));
  tm->time_type = time_type;
}

/**
  Set hour, minute and second of a MYSQL_TIME variable to maximum time value.
  Unlike set_max_time(), does not touch the other structure members.
*/
void set_max_hhmmss(MYSQL_TIME *tm) {
  tm->hour = TIME_MAX_HOUR;
  tm->minute = TIME_MAX_MINUTE;
  tm->second = TIME_MAX_SECOND;
}

/**
  Set MYSQL_TIME variable to maximum time value
  @param tm    OUT  The variable to set.
  @param neg        Sign: 1 if negative, 0 if positive.
*/
void set_max_time(MYSQL_TIME *tm, bool neg) {
  set_zero_time(tm, MYSQL_TIMESTAMP_TIME);
  set_max_hhmmss(tm);
  tm->neg = neg;
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

bool check_date(const MYSQL_TIME *ltime, bool not_zero_date,
                my_time_flags_t flags, int *was_cut) {
  if (not_zero_date) {
    if (((flags & TIME_NO_ZERO_IN_DATE) || !(flags & TIME_FUZZY_DATE)) &&
        (ltime->month == 0 || ltime->day == 0)) {
      *was_cut = MYSQL_TIME_WARN_ZERO_IN_DATE;
      return true;
    } else if ((!(flags & TIME_INVALID_DATES) && ltime->month &&
                ltime->day > days_in_month[ltime->month - 1] &&
                (ltime->month != 2 || calc_days_in_year(ltime->year) != 366 ||
                 ltime->day != 29))) {
      *was_cut = MYSQL_TIME_WARN_OUT_OF_RANGE;
      return true;
    }
  } else if (flags & TIME_NO_ZERO_DATE) {
    *was_cut = MYSQL_TIME_WARN_ZERO_DATE;
    return true;
  }
  return false;
}

/**
  Check if TIME fields are fatally bad and cannot be further adjusted.
  @param ltime  Time value.
  @retval  true   if the value is fatally bad.
  @retval  false  if the value is Ok.
*/
bool check_time_mmssff_range(const MYSQL_TIME *ltime) {
  return ltime->minute >= 60 || ltime->second >= 60 ||
         ltime->second_part > 999999;
}

/**
  Check TIME range. The value can include day part,
  for example:  '1 10:20:30.123456'.

  minute, second and second_part values are not checked
  unless hour is equal TIME_MAX_HOUR.

  @param ltime   Rime value.
  @returns       Test result.
  @retval        false if value is Ok.
  @retval        true if value is out of range.
*/
bool check_time_range_quick(const MYSQL_TIME *ltime) {
  longlong hour = (longlong)ltime->hour + 24LL * ltime->day;
  /* The input value should not be fatally bad */
  DBUG_ASSERT(!check_time_mmssff_range(ltime));
  if (hour <= TIME_MAX_HOUR &&
      (hour != TIME_MAX_HOUR || ltime->minute != TIME_MAX_MINUTE ||
       ltime->second != TIME_MAX_SECOND || !ltime->second_part))
    return false;
  return true;
}

/**
  Check datetime, date, or normalized time (i.e. time without days) range.
  @param ltime   Datetime value.
  @returns
  @retval   false on success
  @retval   true  on error
*/
bool check_datetime_range(const MYSQL_TIME *ltime) {
  /*
    In case of MYSQL_TIMESTAMP_TIME hour value can be up to TIME_MAX_HOUR.
    In case of MYSQL_TIMESTAMP_DATETIME it cannot be bigger than 23.
  */
  return ltime->year > 9999U || ltime->month > 12U || ltime->day > 31U ||
         ltime->minute > 59U || ltime->second > 59U ||
         ltime->second_part > 999999U ||
         (ltime->hour >
          (ltime->time_type == MYSQL_TIMESTAMP_TIME ? TIME_MAX_HOUR : 23U));
}

  /*
    Convert a timestamp string to a MYSQL_TIME value.

    SYNOPSIS
      str_to_datetime()
      str                 String to parse
      length              Length of string
      l_time              Date is stored here
      flags               Bitmap of following items
                          TIME_FUZZY_DATE    Set if we should allow partial
    dates TIME_DATETIME_ONLY Set if we only allow full datetimes.
                          TIME_NO_ZERO_IN_DATE	Don't allow partial dates
                          TIME_NO_ZERO_DATE	Don't allow 0000-00-00 date
                          TIME_INVALID_DATES	Allow 2000-02-31
      status              Conversion status


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

      status->warnings is set to:
      0                            Value OK
      MYSQL_TIME_WARN_TRUNCATED    If value was cut during conversion
      MYSQL_TIME_WARN_OUT_OF_RANGE check_date(date,flags) considers date invalid

      l_time->time_type is set as follows:
      MYSQL_TIMESTAMP_NONE        String wasn't a timestamp, like
                                  [DD [HH:[MM:[SS]]]].fraction.
                                  l_time is not changed.
      MYSQL_TIMESTAMP_DATE        DATE string (YY MM and DD parts ok)
      MYSQL_TIMESTAMP_DATETIME    Full timestamp
      MYSQL_TIMESTAMP_ERROR       Timestamp with wrong values.
                                  All elements in l_time is set to 0
    RETURN VALUES
      0 - Ok
      1 - Error
  */

#define MAX_DATE_PARTS 8

bool str_to_datetime(const char *str, size_t length, MYSQL_TIME *l_time,
                     my_time_flags_t flags, MYSQL_TIME_STATUS *status) {
  uint field_length = 0, year_length = 0, digits, i, number_of_fields;
  uint date[MAX_DATE_PARTS], date_len[MAX_DATE_PARTS];
  uint add_hours = 0, start_loop;
  ulong not_zero_date, allow_space;
  bool is_internal_format;
  const char *pos, *last_field_pos = NULL;
  const char *end = str + length;
  const uchar *format_position;
  bool found_delimitier = 0, found_space = 0;
  uint frac_pos, frac_len;
  DBUG_ENTER("str_to_datetime");
  DBUG_PRINT("ENTER", ("str: %.*s", (int)length, str));

  my_time_status_init(status);

  /* Skip space at start */
  for (; str != end && my_isspace(&my_charset_latin1, *str); str++)
    ;
  if (str == end || !my_isdigit(&my_charset_latin1, *str)) {
    status->warnings = MYSQL_TIME_WARN_TRUNCATED;
    l_time->time_type = MYSQL_TIMESTAMP_NONE;
    DBUG_RETURN(1);
  }

  is_internal_format = 0;
  /* This has to be changed if want to activate different timestamp formats */
  format_position = internal_format_positions;

  /*
    Calculate number of digits in first part.
    If length= 8 or >= 14 then year is of format YYYY.
    (YYYY-MM-DD,  YYYYMMDD, YYYYYMMDDHHMMSS)
  */
  for (pos = str;
       pos != end && (my_isdigit(&my_charset_latin1, *pos) || *pos == 'T');
       pos++)
    ;

  digits = (uint)(pos - str);
  start_loop = 0;                   /* Start of scan loop */
  date_len[format_position[0]] = 0; /* Length of year field */
  if (pos == end || *pos == '.') {
    /* Found date in internal format (only numbers like YYYYMMDD) */
    year_length = (digits == 4 || digits == 8 || digits >= 14) ? 4 : 2;
    field_length = year_length;
    is_internal_format = 1;
    format_position = internal_format_positions;
  } else {
    if (format_position[0] >= 3) /* If year is after HHMMDD */
    {
      /*
        If year is not in first part then we have to determinate if we got
        a date field or a datetime field.
        We do this by checking if there is two numbers separated by
        space in the input.
      */
      while (pos < end && !my_isspace(&my_charset_latin1, *pos)) pos++;
      while (pos < end && !my_isdigit(&my_charset_latin1, *pos)) pos++;
      if (pos == end) {
        if (flags & TIME_DATETIME_ONLY) {
          status->warnings = MYSQL_TIME_WARN_TRUNCATED;
          l_time->time_type = MYSQL_TIMESTAMP_NONE;
          DBUG_RETURN(1); /* Can't be a full datetime */
        }
        /* Date field.  Set hour, minutes and seconds to 0 */
        date[0] = date[1] = date[2] = date[3] = date[4] = 0;
        start_loop = 5; /* Start with first date part */
      }
    }

    field_length = format_position[0] == 0 ? 4 : 2;
  }

  /*
    Only allow space in the first "part" of the datetime field and:
    - after days, part seconds
    - before and after AM/PM (handled by code later)

    2003-03-03 20:00:20 AM
    20:00:20.000000 AM 03-03-2000
  */
  i = MY_MAX((uint)format_position[0], (uint)format_position[1]);
  set_if_bigger(i, (uint)format_position[2]);
  allow_space = ((1 << i) | (1 << format_position[6]));
  allow_space &= (1 | 2 | 4 | 8 | 64);

  not_zero_date = 0;
  for (i = start_loop; i < MAX_DATE_PARTS - 1 && str != end &&
                       my_isdigit(&my_charset_latin1, *str);
       i++) {
    const char *start = str;
    ulong tmp_value = (uint)(uchar)(*str++ - '0');

    /*
      Internal format means no delimiters; every field has a fixed
      width. Otherwise, we scan until we find a delimiter and discard
      leading zeroes -- except for the microsecond part, where leading
      zeroes are significant, and where we never process more than six
      digits.
    */
    bool scan_until_delim = !is_internal_format && (i != format_position[6]);

    while (str != end && my_isdigit(&my_charset_latin1, str[0]) &&
           (scan_until_delim || --field_length)) {
      tmp_value = tmp_value * 10 + (ulong)(uchar)(*str - '0');
      str++;
    }
    date_len[i] = (uint)(str - start);
    if (tmp_value > 999999) /* Impossible date part */
    {
      status->warnings = MYSQL_TIME_WARN_TRUNCATED;
      l_time->time_type = MYSQL_TIMESTAMP_NONE;
      DBUG_RETURN(1);
    }
    date[i] = tmp_value;
    not_zero_date |= tmp_value;

    /* Length of next field */
    field_length = format_position[i + 1] == 0 ? 4 : 2;

    if ((last_field_pos = str) == end) {
      i++; /* Register last found part */
      break;
    }
    /* Allow a 'T' after day to allow CCYYMMDDT type of fields */
    if (i == format_position[2] && *str == 'T') {
      str++; /* ISO8601:  CCYYMMDDThhmmss */
      continue;
    }
    if (i == format_position[5]) /* Seconds */
    {
      if (*str == '.') /* Followed by part seconds */
      {
        str++;
        /*
          Shift last_field_pos, so '2001-01-01 00:00:00.'
          is treated as a valid value
        */
        last_field_pos = str;
        field_length = 6; /* 6 digits */
      } else if (my_isdigit(&my_charset_latin1, str[0])) {
        /*
          We do not see a decimal point which would have indicated a
          fractional second part in further read. So we skip the further
          processing of digits.
        */
        i++;
        break;
      }
      continue;
    }
    while (str != end && (my_ispunct(&my_charset_latin1, *str) ||
                          my_isspace(&my_charset_latin1, *str))) {
      if (my_isspace(&my_charset_latin1, *str)) {
        if (!(allow_space & (1 << i))) {
          status->warnings = MYSQL_TIME_WARN_TRUNCATED;
          l_time->time_type = MYSQL_TIMESTAMP_NONE;
          DBUG_RETURN(1);
        }
        found_space = 1;
      }
      str++;
      found_delimitier = 1; /* Should be a 'normal' date */
    }
    /* Check if next position is AM/PM */
    if (i == format_position[6]) /* Seconds, time for AM/PM */
    {
      i++;                           /* Skip AM/PM part */
      if (format_position[7] != 255) /* If using AM/PM */
      {
        if (str + 2 <= end && (str[1] == 'M' || str[1] == 'm')) {
          if (str[0] == 'p' || str[0] == 'P')
            add_hours = 12;
          else if (str[0] != 'a' && str[0] != 'A')
            continue; /* Not AM/PM */
          str += 2;   /* Skip AM/PM */
          /* Skip space after AM/PM */
          while (str != end && my_isspace(&my_charset_latin1, *str)) str++;
        }
      }
    }
    last_field_pos = str;
  }
  if (found_delimitier && !found_space && (flags & TIME_DATETIME_ONLY)) {
    status->warnings = MYSQL_TIME_WARN_TRUNCATED;
    l_time->time_type = MYSQL_TIMESTAMP_NONE;
    DBUG_RETURN(1); /* Can't be a datetime */
  }

  str = last_field_pos;

  number_of_fields = i - start_loop;
  while (i < MAX_DATE_PARTS) {
    date_len[i] = 0;
    date[i++] = 0;
  }

  if (!is_internal_format) {
    year_length = date_len[(uint)format_position[0]];
    if (!year_length) /* Year must be specified */
    {
      status->warnings = MYSQL_TIME_WARN_TRUNCATED;
      l_time->time_type = MYSQL_TIMESTAMP_NONE;
      DBUG_RETURN(1);
    }

    l_time->year = date[(uint)format_position[0]];
    l_time->month = date[(uint)format_position[1]];
    l_time->day = date[(uint)format_position[2]];
    l_time->hour = date[(uint)format_position[3]];
    l_time->minute = date[(uint)format_position[4]];
    l_time->second = date[(uint)format_position[5]];

    frac_pos = (uint)format_position[6];
    frac_len = date_len[frac_pos];
    status->fractional_digits = frac_len;
    if (frac_len < 6)
      date[frac_pos] *= (uint)log_10_int[DATETIME_MAX_DECIMALS - frac_len];
    l_time->second_part = date[frac_pos];

    if (format_position[7] != (uchar)255) {
      if (l_time->hour > 12) {
        status->warnings = MYSQL_TIME_WARN_TRUNCATED;
        goto err;
      }
      l_time->hour = l_time->hour % 12 + add_hours;
    }
  } else {
    l_time->year = date[0];
    l_time->month = date[1];
    l_time->day = date[2];
    l_time->hour = date[3];
    l_time->minute = date[4];
    l_time->second = date[5];
    if (date_len[6] < 6)
      date[6] *= (uint)log_10_int[DATETIME_MAX_DECIMALS - date_len[6]];
    l_time->second_part = date[6];
    status->fractional_digits = date_len[6];
  }
  l_time->neg = 0;

  if (year_length == 2 && not_zero_date)
    l_time->year += (l_time->year < YY_PART_YEAR ? 2000 : 1900);

  /*
    Set time_type before check_datetime_range(),
    as the latter relies on initialized time_type value.
  */
  l_time->time_type =
      (number_of_fields <= 3 ? MYSQL_TIMESTAMP_DATE : MYSQL_TIMESTAMP_DATETIME);

  if (number_of_fields < 3 || check_datetime_range(l_time)) {
    /* Only give warning for a zero date if there is some garbage after */
    if (!not_zero_date) /* If zero date */
    {
      for (; str != end; str++) {
        if (!my_isspace(&my_charset_latin1, *str)) {
          not_zero_date = 1; /* Give warning */
          break;
        }
      }
    }
    status->warnings |=
        not_zero_date ? MYSQL_TIME_WARN_TRUNCATED : MYSQL_TIME_WARN_ZERO_DATE;
    goto err;
  }

  if (check_date(l_time, not_zero_date != 0, flags, &status->warnings))
    goto err;

  /* Scan all digits left after microseconds */
  if (status->fractional_digits == 6 && str != end) {
    if (my_isdigit(&my_charset_latin1, *str)) {
      /*
        We don't need the exact nanoseconds value.
        Knowing the first digit is enough for rounding.
      */
      status->nanoseconds = 100 * (int)(*str++ - '0');
      for (; str != end && my_isdigit(&my_charset_latin1, *str); str++) {
      }
    }
  }

  for (; str != end; str++) {
    if (!my_isspace(&my_charset_latin1, *str)) {
      status->warnings = MYSQL_TIME_WARN_TRUNCATED;
      break;
    }
  }

  DBUG_RETURN(0);

err:
  set_zero_time(l_time, MYSQL_TIMESTAMP_ERROR);
  DBUG_RETURN(1);
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
   status               Conversion status

   status.warning is set to:
     MYSQL_TIME_WARN_TRUNCATED flag if the input string
                        was cut during conversion, and/or
     MYSQL_TIME_WARN_OUT_OF_RANGE flag, if the value is out of range.

   NOTES
     Because of the extra days argument, this function can only
     work with times where the time arguments are in the above order.

   RETURN
     0  ok
     1  error
*/

bool str_to_time(const char *str, size_t length, MYSQL_TIME *l_time,
                 MYSQL_TIME_STATUS *status) {
  ulong date[5];
  ulonglong value;
  const char *end = str + length, *end_of_days;
  bool found_days, found_hours;
  uint state;
  const char *start;

  my_time_status_init(status);
  l_time->neg = 0;
  for (; str != end && my_isspace(&my_charset_latin1, *str); str++) length--;
  if (str != end && *str == '-') {
    l_time->neg = 1;
    str++;
    length--;
  }
  if (str == end) return 1;

  // Remember beginning of first non-space/- char.
  start = str;

  /* Check first if this is a full TIMESTAMP */
  if (length >= 12) { /* Probably full timestamp */
    (void)str_to_datetime(str, length, l_time,
                          (TIME_FUZZY_DATE | TIME_DATETIME_ONLY), status);
    if (l_time->time_type >= MYSQL_TIMESTAMP_ERROR)
      return l_time->time_type == MYSQL_TIMESTAMP_ERROR;
    my_time_status_init(status);
  }

  /* Not a timestamp. Try to get this as a DAYS_TO_SECOND string */
  for (value = 0; str != end && my_isdigit(&my_charset_latin1, *str); str++)
    value = value * 10L + (long)(*str - '0');

  if (value > UINT_MAX) return 1;

  /* Skip all space after 'days' */
  end_of_days = str;
  for (; str != end && my_isspace(&my_charset_latin1, str[0]); str++)
    ;

  state = 0;
  found_days = found_hours = 0;
  if ((uint)(end - str) > 1 && str != end_of_days &&
      my_isdigit(&my_charset_latin1, *str)) { /* Found days part */
    date[0] = (ulong)value;
    state = 1; /* Assume next is hours */
    found_days = 1;
  } else if ((end - str) > 1 && *str == time_separator &&
             my_isdigit(&my_charset_latin1, str[1])) {
    date[0] = 0; /* Assume we found hours */
    date[1] = (ulong)value;
    state = 2;
    found_hours = 1;
    str++; /* skip ':' */
  } else {
    /* String given as one number; assume HHMMSS format */
    date[0] = 0;
    date[1] = (ulong)(value / 10000);
    date[2] = (ulong)(value / 100 % 100);
    date[3] = (ulong)(value % 100);
    state = 4;
    goto fractional;
  }

  /* Read hours, minutes and seconds */
  for (;;) {
    for (value = 0; str != end && my_isdigit(&my_charset_latin1, *str); str++)
      value = value * 10L + (long)(*str - '0');
    date[state++] = (ulong)value;
    if (state == 4 || (end - str) < 2 || *str != time_separator ||
        !my_isdigit(&my_charset_latin1, str[1]))
      break;
    str++; /* Skip time_separator (':') */
  }

  if (state != 4) { /* Not HH:MM:SS */
    /* Fix the date to assume that seconds was given */
    if (!found_hours && !found_days) {
      size_t len = sizeof(long) * (state - 1);
      memmove((uchar *)(date + 4) - len, (uchar *)(date + state) - len, len);
      memset(date, 0, sizeof(long) * (4 - state));
    } else
      memset((date + state), 0, sizeof(long) * (4 - state));
  }

fractional:
  /* Get fractional second part */
  if ((end - str) >= 2 && *str == '.' &&
      my_isdigit(&my_charset_latin1, str[1])) {
    int field_length = 5;
    str++;
    value = (uint)(uchar)(*str - '0');
    while (++str != end && my_isdigit(&my_charset_latin1, *str)) {
      if (field_length-- > 0) value = value * 10 + (uint)(uchar)(*str - '0');
    }
    if (field_length >= 0) {
      status->fractional_digits = DATETIME_MAX_DECIMALS - field_length;
      if (field_length > 0) value *= (long)log_10_int[field_length];
    } else {
      /* Scan digits left after microseconds */
      status->fractional_digits = 6;
      status->nanoseconds = 100 * (int)(str[-1] - '0');
      for (; str != end && my_isdigit(&my_charset_latin1, *str); str++) {
      }
    }
    date[4] = (ulong)value;
  } else if ((end - str) == 1 && *str == '.') {
    str++;
    date[4] = 0;
  } else
    date[4] = 0;

  /* Check for exponent part: E<gigit> | E<sign><digit> */
  /* (may occur as result of %g formatting of time value) */
  if ((end - str) > 1 && (*str == 'e' || *str == 'E') &&
      (my_isdigit(&my_charset_latin1, str[1]) ||
       ((str[1] == '-' || str[1] == '+') && (end - str) > 2 &&
        my_isdigit(&my_charset_latin1, str[2]))))
    return 1;

  if (internal_format_positions[7] != 255) {
    /* Read a possible AM/PM */
    while (str != end && my_isspace(&my_charset_latin1, *str)) str++;
    if (str + 2 <= end && (str[1] == 'M' || str[1] == 'm')) {
      if (str[0] == 'p' || str[0] == 'P') {
        str += 2;
        date[1] = date[1] % 12 + 12;
      } else if (str[0] == 'a' || str[0] == 'A')
        str += 2;
    }
  }

  /* Integer overflow checks */
  if (date[0] > UINT_MAX || date[1] > UINT_MAX || date[2] > UINT_MAX ||
      date[3] > UINT_MAX || date[4] > UINT_MAX)
    return 1;

  l_time->year = 0; /* For protocol::store_time */
  l_time->month = 0;

  l_time->day = 0;
  l_time->hour = date[1] + date[0] * 24; /* Mix days and hours */

  l_time->minute = date[2];
  l_time->second = date[3];
  l_time->second_part = date[4];
  l_time->time_type = MYSQL_TIMESTAMP_TIME;

  if (check_time_mmssff_range(l_time)) {
    status->warnings |= MYSQL_TIME_WARN_OUT_OF_RANGE;
    return true;
  }

  /* Adjust the value into supported MYSQL_TIME range */
  adjust_time_range(l_time, &status->warnings);

  /* Check if there is garbage at end of the MYSQL_TIME specification */
  if (str != end) {
    do {
      if (!my_isspace(&my_charset_latin1, *str)) {
        status->warnings |= MYSQL_TIME_WARN_TRUNCATED;
        // No char was actually used in conversion - bad value
        if (str == start) {
          l_time->time_type = MYSQL_TIMESTAMP_NONE;
          return true;
        }
        break;
      }
    } while (++str != end);
  }
  return 0;
}

/**
  Convert number to TIME
  @param nr            Number to convert.
  @param [out] ltime     Variable to convert to.
  @param [out] warnings  Warning vector.

  @retval false OK
  @retval true No. is out of range
*/
bool number_to_time(longlong nr, MYSQL_TIME *ltime, int *warnings) {
  if (nr > TIME_MAX_VALUE) {
    /* For huge numbers try full DATETIME, like str_to_time does. */
    if (nr >= 10000000000LL) /* '0001-00-00 00-00-00' */
    {
      int warnings_backup = *warnings;
      if (number_to_datetime(nr, ltime, 0, warnings) != -1LL) return false;
      *warnings = warnings_backup;
    }
    set_max_time(ltime, 0);
    *warnings |= MYSQL_TIME_WARN_OUT_OF_RANGE;
    return true;
  } else if (nr < -TIME_MAX_VALUE) {
    set_max_time(ltime, 1);
    *warnings |= MYSQL_TIME_WARN_OUT_OF_RANGE;
    return true;
  }
  if ((ltime->neg = (nr < 0))) nr = -nr;
  if (nr % 100 >= 60 || nr / 100 % 100 >= 60) /* Check hours and minutes */
  {
    set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);
    *warnings |= MYSQL_TIME_WARN_OUT_OF_RANGE;
    return true;
  }
  ltime->time_type = MYSQL_TIMESTAMP_TIME;
  ltime->year = ltime->month = ltime->day = 0;
  TIME_set_hhmmss(ltime, (uint)nr);
  ltime->second_part = 0;
  return false;
}

/**
  Adjust 'time' value to lie in the MYSQL_TIME range.
  If the time value lies outside of the range [-838:59:59, 838:59:59],
  set it to the closest endpoint of the range and set
  MYSQL_TIME_WARN_OUT_OF_RANGE flag in the 'warning' variable.

  @param  my_time  pointer to MYSQL_TIME value
  @param  warning  set MYSQL_TIME_WARN_OUT_OF_RANGE flag if the value is out of
  range
*/
void adjust_time_range(MYSQL_TIME *my_time, int *warning) {
  DBUG_ASSERT(!check_time_mmssff_range(my_time));
  if (check_time_range_quick(my_time)) {
    my_time->day = my_time->second_part = 0;
    set_max_hhmmss(my_time);
    *warning |= MYSQL_TIME_WARN_OUT_OF_RANGE;
  }
}

/*
  Prepare offset of system time zone from UTC for my_system_gmt_sec() func.

  SYNOPSIS
    my_init_time()
*/
void my_init_time(void) {
  time_t seconds;
  struct tm *l_time, tm_tmp;
  MYSQL_TIME my_time;
  bool not_used;

  seconds = (time_t)time((time_t *)0);
  localtime_r(&seconds, &tm_tmp);
  l_time = &tm_tmp;
  my_time_zone = 3600; /* Comp. for -3600 in my_gmt_sec */
  my_time.year = (uint)l_time->tm_year + 1900;
  my_time.month = (uint)l_time->tm_mon + 1;
  my_time.day = (uint)l_time->tm_mday;
  my_time.hour = (uint)l_time->tm_hour;
  my_time.minute = (uint)l_time->tm_min;
  my_time.second = (uint)l_time->tm_sec;
  my_time.time_type = MYSQL_TIMESTAMP_DATETIME;
  my_time.neg = 0;
  my_time.second_part = 0;
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

uint year_2000_handling(uint year) {
  if ((year = year + 1900) < 1900 + YY_PART_YEAR) year += 100;
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

long calc_daynr(uint year, uint month, uint day) {
  long delsum;
  int temp;
  int y = year; /* may be < 0 temporarily */
  DBUG_ENTER("calc_daynr");

  if (y == 0 && month == 0) DBUG_RETURN(0); /* Skip errors */
  /* Cast to int to be able to handle month == 0 */
  delsum = (long)(365 * y + 31 * ((int)month - 1) + (int)day);
  if (month <= 2)
    y--;
  else
    delsum -= (long)((int)month * 4 + 23) / 10;
  temp = (int)((y / 100 + 1) * 3) / 4;
  DBUG_PRINT("exit", ("year: %d  month: %d  day: %d -> daynr: %ld",
                      y + (month <= 2), month, day, delsum + y / 4 - temp));
  DBUG_ASSERT(delsum + (int)y / 4 - temp >= 0);
  DBUG_RETURN(delsum + (int)y / 4 - temp);
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
my_time_t my_system_gmt_sec(const MYSQL_TIME *t_src, long *my_timezone,
                            bool *in_dst_time_gap) {
  uint loop;
  time_t tmp = 0;
  int shift = 0;
  MYSQL_TIME tmp_time;
  MYSQL_TIME *t = &tmp_time;
  struct tm *l_time, tm_tmp;
  long diff, current_timezone;

  /*
    Use temp variable to avoid trashing input data, which could happen in
    case of shift required for boundary dates processing.
  */
  memcpy(&tmp_time, t_src, sizeof(MYSQL_TIME));

  if (!validate_timestamp_range(t)) return 0;

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
  if ((t->year == TIMESTAMP_MAX_YEAR) && (t->month == 1) && (t->day > 4)) {
    /*
      Below we will pass (uint) (t->day - shift) to calc_daynr.
      As we don't want to get an overflow here, we will shift
      only safe dates. That's why we have (t->day > 4) above.
    */
    t->day -= 2;
    shift = 2;
  }

  tmp = (time_t)(((calc_daynr((uint)t->year, (uint)t->month, (uint)t->day) -
                   (long)days_at_timestart) *
                      SECONDS_IN_24H +
                  (long)t->hour * 3600L + (long)(t->minute * 60 + t->second)) +
                 (time_t)my_time_zone - 3600);

  current_timezone = my_time_zone;
  localtime_r(&tmp, &tm_tmp);
  l_time = &tm_tmp;
  for (loop = 0; loop < 2 && (t->hour != (uint)l_time->tm_hour ||
                              t->minute != (uint)l_time->tm_min ||
                              t->second != (uint)l_time->tm_sec);
       loop++) { /* One check should be enough ? */
    /* Get difference in days */
    int days = t->day - l_time->tm_mday;
    if (days < -1)
      days = 1; /* Month has wrapped */
    else if (days > 1)
      days = -1;
    diff = (3600L * (long)(days * 24 + ((int)t->hour - (int)l_time->tm_hour)) +
            (long)(60 * ((int)t->minute - (int)l_time->tm_min)) +
            (long)((int)t->second - (int)l_time->tm_sec));
    current_timezone += diff + 3600; /* Compensate for -3600 above */
    tmp += (time_t)diff;
    localtime_r(&tmp, &tm_tmp);
    l_time = &tm_tmp;
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
  if (loop == 2 && t->hour != (uint)l_time->tm_hour) {
    int days = t->day - l_time->tm_mday;
    if (days < -1)
      days = 1; /* Month has wrapped */
    else if (days > 1)
      days = -1;
    diff = (3600L * (long)(days * 24 + ((int)t->hour - (int)l_time->tm_hour)) +
            (long)(60 * ((int)t->minute - (int)l_time->tm_min)) +
            (long)((int)t->second - (int)l_time->tm_sec));
    if (diff == 3600)
      tmp += 3600 - t->minute * 60 - t->second; /* Move to next hour */
    else if (diff == -3600)
      tmp -= t->minute * 60 + t->second; /* Move to previous hour */

    *in_dst_time_gap = 1;
  }
  *my_timezone = current_timezone;

  /* shift back, if we were dealing with boundary dates */
  tmp += shift * SECONDS_IN_24H;

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
  if (!IS_TIME_T_VALID_FOR_TIMESTAMP(tmp)) tmp = 0;

  return (my_time_t)tmp;
} /* my_system_gmt_sec */

/**
  Print the microsecond part: ".NNN"
  @param to        OUT The string pointer to print at
  @param useconds      The microseconds value.
  @param dec           Precision, between 1 and 6.
  @return              The length of the result string.
*/
static inline int my_useconds_to_str(char *to, ulong useconds, uint dec) {
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  return sprintf(to, ".%0*lu", (int)dec,
                 useconds / (ulong)log_10_int[DATETIME_MAX_DECIMALS - dec]);
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

int my_time_to_str(const MYSQL_TIME *l_time, char *to, uint dec) {
  uint extra_hours = 0;
  int len = sprintf(to, "%s%02u:%02u:%02u", (l_time->neg ? "-" : ""),
                    extra_hours + l_time->hour, l_time->minute, l_time->second);
  if (dec) len += my_useconds_to_str(to + len, l_time->second_part, dec);
  return len;
}

int my_date_to_str(const MYSQL_TIME *l_time, char *to) {
  return sprintf(to, "%04u-%02u-%02u", l_time->year, l_time->month,
                 l_time->day);
}

/*
  Convert datetime to a string 'YYYY-MM-DD hh:mm:ss'.
  Open coded for better performance.
  This code previously resided in field.cc, in Field_timestamp::val_str().

  @param  to     OUT  The string pointer to print at.
  @param  ltime       The MYSQL_TIME value.
  @return             The length of the result string.
*/
static inline int TIME_to_datetime_str(char *to, const MYSQL_TIME *ltime) {
  uint32 temp, temp2;
  /* Year */
  temp = ltime->year / 100;
  *to++ = (char)('0' + temp / 10);
  *to++ = (char)('0' + temp % 10);
  temp = ltime->year % 100;
  *to++ = (char)('0' + temp / 10);
  *to++ = (char)('0' + temp % 10);
  *to++ = '-';
  /* Month */
  temp = ltime->month;
  temp2 = temp / 10;
  temp = temp - temp2 * 10;
  *to++ = (char)('0' + (char)(temp2));
  *to++ = (char)('0' + (char)(temp));
  *to++ = '-';
  /* Day */
  temp = ltime->day;
  temp2 = temp / 10;
  temp = temp - temp2 * 10;
  *to++ = (char)('0' + (char)(temp2));
  *to++ = (char)('0' + (char)(temp));
  *to++ = ' ';
  /* Hour */
  temp = ltime->hour;
  temp2 = temp / 10;
  temp = temp - temp2 * 10;
  *to++ = (char)('0' + (char)(temp2));
  *to++ = (char)('0' + (char)(temp));
  *to++ = ':';
  /* Minute */
  temp = ltime->minute;
  temp2 = temp / 10;
  temp = temp - temp2 * 10;
  *to++ = (char)('0' + (char)(temp2));
  *to++ = (char)('0' + (char)(temp));
  *to++ = ':';
  /* Second */
  temp = ltime->second;
  temp2 = temp / 10;
  temp = temp - temp2 * 10;
  *to++ = (char)('0' + (char)(temp2));
  *to++ = (char)('0' + (char)(temp));
  return 19;
}

/**
  Print a datetime value with an optional fractional part.

  @param l_time       The MYSQL_TIME value to print
  @param [out] to     The string pointer to print at
  @param dec          Precision, in the range 0..6
  @return       The length of the result string.
*/
int my_datetime_to_str(const MYSQL_TIME *l_time, char *to, uint dec) {
  int len = TIME_to_datetime_str(to, l_time);
  if (dec)
    len += my_useconds_to_str(to + len, l_time->second_part, dec);
  else
    to[len] = '\0';
  return len;
}

/*
  Convert struct DATE/TIME/DATETIME value to string using built-in
  MySQL time conversion formats.

  SYNOPSIS
    my_TIME_to_string()

  NOTE
    The string must have at least MAX_DATE_STRING_REP_LENGTH bytes reserved.
*/

int my_TIME_to_str(const MYSQL_TIME *l_time, char *to, uint dec) {
  switch (l_time->time_type) {
    case MYSQL_TIMESTAMP_DATETIME:
      return my_datetime_to_str(l_time, to, dec);
    case MYSQL_TIMESTAMP_DATE:
      return my_date_to_str(l_time, to);
    case MYSQL_TIMESTAMP_TIME:
      return my_time_to_str(l_time, to, dec);
    case MYSQL_TIMESTAMP_NONE:
    case MYSQL_TIMESTAMP_ERROR:
      to[0] = '\0';
      return 0;
    default:
      DBUG_ASSERT(0);
      return 0;
  }
}

/**
  Print a timestamp with an oprional fractional part: XXXXX[.YYYYY]

  @param      tm  The timestamp value to print.
  @param [out] to  The string pointer to print at.
  @param      dec Precision, in the range 0..6.
  @return         The length of the result string.
*/
int my_timeval_to_str(const struct timeval *tm, char *to, uint dec) {
  int len = sprintf(to, "%d", (int)tm->tv_sec);
  if (dec) len += my_useconds_to_str(to + len, tm->tv_usec, dec);
  return len;
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

    was_cut         if return value -1: one of
                      - MYSQL_TIME_WARN_OUT_OF_RANGE
                      - MYSQL_TIME_WARN_ZERO_DATE
                      - MYSQL_TIME_WARN_TRUNCATED
                    otherwise 0.
*/

longlong number_to_datetime(longlong nr, MYSQL_TIME *time_res,
                            my_time_flags_t flags, int *was_cut) {
  long part1, part2;

  *was_cut = 0;
  memset(time_res, 0, sizeof(*time_res));
  time_res->time_type = MYSQL_TIMESTAMP_DATE;

  if (nr == 0LL || nr >= 10000101000000LL) {
    time_res->time_type = MYSQL_TIMESTAMP_DATETIME;
    if (nr > 99999999999999LL) /* 9999-99-99 99:99:99 */
    {
      *was_cut = MYSQL_TIME_WARN_OUT_OF_RANGE;
      return -1LL;
    }
    goto ok;
  }
  if (nr < 101) goto err;
  if (nr <= (YY_PART_YEAR - 1) * 10000L + 1231L) {
    nr = (nr + 20000000L) * 1000000L; /* YYMMDD, year: 2000-2069 */
    goto ok;
  }
  if (nr < (YY_PART_YEAR)*10000L + 101L) goto err;
  if (nr <= 991231L) {
    nr = (nr + 19000000L) * 1000000L; /* YYMMDD, year: 1970-1999 */
    goto ok;
  }
  /*
    Though officially we support DATE values from 1000-01-01 only, one can
    easily insert a value like 1-1-1. So, for consistency reasons such dates
    are allowed when TIME_FUZZY_DATE is set.
  */
  if (nr < 10000101L && !(flags & TIME_FUZZY_DATE)) goto err;
  if (nr <= 99991231L) {
    nr = nr * 1000000L;
    goto ok;
  }
  if (nr < 101000000L) goto err;

  time_res->time_type = MYSQL_TIMESTAMP_DATETIME;

  if (nr <= (YY_PART_YEAR - 1) * 10000000000LL + 1231235959LL) {
    nr = nr + 20000000000000LL; /* YYMMDDHHMMSS, 2000-2069 */
    goto ok;
  }
  if (nr < YY_PART_YEAR * 10000000000LL + 101000000LL) goto err;
  if (nr <= 991231235959LL)
    nr = nr + 19000000000000LL; /* YYMMDDHHMMSS, 1970-1999 */

ok:
  part1 = (long)(nr / 1000000LL);
  part2 = (long)(nr - (longlong)part1 * 1000000LL);
  time_res->year = (int)(part1 / 10000L);
  part1 %= 10000L;
  time_res->month = (int)part1 / 100;
  time_res->day = (int)part1 % 100;
  time_res->hour = (int)(part2 / 10000L);
  part2 %= 10000L;
  time_res->minute = (int)part2 / 100;
  time_res->second = (int)part2 % 100;

  if (!check_datetime_range(time_res) &&
      !check_date(time_res, (nr != 0), flags, was_cut))
    return nr;

  /* Don't want to have was_cut get set if TIME_NO_ZERO_DATE was violated. */
  if (!nr && (flags & TIME_NO_ZERO_DATE)) return -1LL;

err:
  *was_cut = MYSQL_TIME_WARN_TRUNCATED;
  return -1LL;
}

/**
  Convert time value to integer in YYYYMMDDHHMMSS.
  @param  my_time  The MYSQL_TIME value to convert.
  @return          A number in format YYYYMMDDHHMMSS.
*/
ulonglong TIME_to_ulonglong_datetime(const MYSQL_TIME *my_time) {
  return ((ulonglong)(my_time->year * 10000UL + my_time->month * 100UL +
                      my_time->day) *
              1000000ULL +
          (ulonglong)(my_time->hour * 10000UL + my_time->minute * 100UL +
                      my_time->second));
}

/**
  Convert MYSQL_TIME value to integer in YYYYMMDD format
  @param my_time  The MYSQL_TIME value to convert.
  @return         A number in format YYYYMMDD.
*/
ulonglong TIME_to_ulonglong_date(const MYSQL_TIME *my_time) {
  return (ulonglong)(my_time->year * 10000UL + my_time->month * 100UL +
                     my_time->day);
}

/**
  Convert MYSQL_TIME value to integer in HHMMSS format.
  This function doesn't take into account time->day member:
  it's assumed that days have been converted to hours already.
  @param my_time  The TIME value to convert.
  @return         The number in HHMMSS format.
*/
ulonglong TIME_to_ulonglong_time(const MYSQL_TIME *my_time) {
  return (ulonglong)(my_time->hour * 10000UL + my_time->minute * 100UL +
                     my_time->second);
}

/**
  Set day, month and year from a number
  @param ltime    MYSQL_TIME variable
  @param yymmdd   Number in YYYYMMDD format
*/
void TIME_set_yymmdd(MYSQL_TIME *ltime, uint yymmdd) {
  ltime->day = (int)(yymmdd % 100);
  ltime->month = (int)(yymmdd / 100) % 100;
  ltime->year = (int)(yymmdd / 10000);
}

/**
  Set hour, minute and secondr from a number
  @param ltime    MYSQL_TIME variable
  @param hhmmss   Number in HHMMSS format
*/
void TIME_set_hhmmss(MYSQL_TIME *ltime, uint hhmmss) {
  ltime->second = (int)(hhmmss % 100);
  ltime->minute = (int)(hhmmss / 100) % 100;
  ltime->hour = (int)(hhmmss / 10000);
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

ulonglong TIME_to_ulonglong(const MYSQL_TIME *my_time) {
  switch (my_time->time_type) {
    case MYSQL_TIMESTAMP_DATETIME:
      return TIME_to_ulonglong_datetime(my_time);
    case MYSQL_TIMESTAMP_DATE:
      return TIME_to_ulonglong_date(my_time);
    case MYSQL_TIMESTAMP_TIME:
      return TIME_to_ulonglong_time(my_time);
    case MYSQL_TIMESTAMP_NONE:
    case MYSQL_TIMESTAMP_ERROR:
      return 0ULL;
    default:
      DBUG_ASSERT(0);
  }
  return 0;
}

/*** TIME low-level memory and disk representation routines ***/

/*
  In-memory format:

   1  bit sign          (Used for sign, when on disk)
   1  bit unused        (Reserved for wider hour range, e.g. for intervals)
   10 bit hour          (0-836)
   6  bit minute        (0-59)
   6  bit second        (0-59)
  24  bits microseconds (0-999999)

 Total: 48 bits = 6 bytes
   Suhhhhhh.hhhhmmmm.mmssssss.ffffffff.ffffffff.ffffffff
*/

/**
  Convert time value to numeric packed representation.

  @param    ltime   The value to convert.
  @return           Numeric packed representation.
*/
longlong TIME_to_longlong_time_packed(const MYSQL_TIME *ltime) {
  /* If month is 0, we mix day with hours: "1 00:10:10" -> "24:00:10" */
  long hms = (((ltime->month ? 0 : ltime->day * 24) + ltime->hour) << 12) |
             (ltime->minute << 6) | ltime->second;
  longlong tmp = MY_PACKED_TIME_MAKE(hms, ltime->second_part);
  return ltime->neg ? -tmp : tmp;
}

/**
  Convert time packed numeric representation to time.

  @param [out] ltime  The MYSQL_TIME variable to set.
  @param      tmp    The packed numeric representation.
*/
void TIME_from_longlong_time_packed(MYSQL_TIME *ltime, longlong tmp) {
  longlong hms;
  if ((ltime->neg = (tmp < 0))) tmp = -tmp;
  hms = MY_PACKED_TIME_GET_INT_PART(tmp);
  ltime->year = (uint)0;
  ltime->month = (uint)0;
  ltime->day = (uint)0;
  ltime->hour = (uint)(hms >> 12) % (1 << 10); /* 10 bits starting at 12th */
  ltime->minute = (uint)(hms >> 6) % (1 << 6); /* 6 bits starting at 6th   */
  ltime->second = (uint)hms % (1 << 6);        /* 6 bits starting at 0th   */
  ltime->second_part = MY_PACKED_TIME_GET_FRAC_PART(tmp);
  ltime->time_type = MYSQL_TIMESTAMP_TIME;
}

/*
  On disk we convert from signed representation to unsigned
  representation using TIMEF_OFS, so all values become binary comparable.
*/
#define TIMEF_OFS 0x800000000000LL
#define TIMEF_INT_OFS 0x800000LL

/**
  Convert in-memory numeric time representation to on-disk representation

  @param       nr   Value in packed numeric time format.
  @param [out] ptr  The buffer to put value at.
  @param       dec  Precision.
*/
void my_time_packed_to_binary(longlong nr, uchar *ptr, uint dec) {
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  /* Make sure the stored value was previously properly rounded or truncated */
  DBUG_ASSERT((MY_PACKED_TIME_GET_FRAC_PART(nr) %
               (int)log_10_int[DATETIME_MAX_DECIMALS - dec]) == 0);

  switch (dec) {
    case 0:
    default:
      mi_int3store(ptr, TIMEF_INT_OFS + MY_PACKED_TIME_GET_INT_PART(nr));
      break;

    case 1:
    case 2:
      mi_int3store(ptr, TIMEF_INT_OFS + MY_PACKED_TIME_GET_INT_PART(nr));
      ptr[3] = (unsigned char)(char)(MY_PACKED_TIME_GET_FRAC_PART(nr) / 10000);
      break;

    case 4:
    case 3:
      mi_int3store(ptr, TIMEF_INT_OFS + MY_PACKED_TIME_GET_INT_PART(nr));
      mi_int2store(ptr + 3, MY_PACKED_TIME_GET_FRAC_PART(nr) / 100);
      break;

    case 5:
    case 6:
      mi_int6store(ptr, nr + TIMEF_OFS);
      break;
  }
}

/**
  Convert on-disk time representation to in-memory packed numeric
  representation.

  @param   ptr  The pointer to read the value at.
  @param   dec  Precision.
  @return       Packed numeric time representation.
*/
longlong my_time_packed_from_binary(const uchar *ptr, uint dec) {
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);

  switch (dec) {
    case 0:
    default: {
      longlong intpart = mi_uint3korr(ptr) - TIMEF_INT_OFS;
      return MY_PACKED_TIME_MAKE_INT(intpart);
    }
    case 1:
    case 2: {
      longlong intpart = mi_uint3korr(ptr) - TIMEF_INT_OFS;
      int frac = (uint)ptr[3];
      if (intpart < 0 && frac) {
        /*
          Negative values are stored with reverse fractional part order,
          for binary sort compatibility.

            Disk value  intpart frac   Time value   Memory value
            800000.00    0      0      00:00:00.00  0000000000.000000
            7FFFFF.FF   -1      255   -00:00:00.01  FFFFFFFFFF.FFD8F0
            7FFFFF.9D   -1      99    -00:00:00.99  FFFFFFFFFF.F0E4D0
            7FFFFF.00   -1      0     -00:00:01.00  FFFFFFFFFF.000000
            7FFFFE.FF   -1      255   -00:00:01.01  FFFFFFFFFE.FFD8F0
            7FFFFE.F6   -2      246   -00:00:01.10  FFFFFFFFFE.FE7960

            Formula to convert fractional part from disk format
            (now stored in "frac" variable) to absolute value: "0x100 - frac".
            To reconstruct in-memory value, we shift
            to the next integer value and then substruct fractional part.
        */
        intpart++;     /* Shift to the next integer value */
        frac -= 0x100; /* -(0x100 - frac) */
      }
      return MY_PACKED_TIME_MAKE(intpart, frac * 10000);
    }

    case 3:
    case 4: {
      longlong intpart = mi_uint3korr(ptr) - TIMEF_INT_OFS;
      int frac = mi_uint2korr(ptr + 3);
      if (intpart < 0 && frac) {
        /*
          Fix reverse fractional part order: "0x10000 - frac".
          See comments for FSP=1 and FSP=2 above.
        */
        intpart++;       /* Shift to the next integer value */
        frac -= 0x10000; /* -(0x10000-frac) */
      }
      return MY_PACKED_TIME_MAKE(intpart, frac * 100);
    }

    case 5:
    case 6:
      return ((longlong)mi_uint6korr(ptr)) - TIMEF_OFS;
  }
}

/*** DATETIME and DATE low-level memory and disk representation routines ***/

/*
    1 bit  sign            (used when on disk)
   17 bits year*13+month   (year 0-9999, month 0-12)
    5 bits day             (0-31)
    5 bits hour            (0-23)
    6 bits minute          (0-59)
    6 bits second          (0-59)
   24 bits microseconds    (0-999999)

   Total: 64 bits = 8 bytes

   SYYYYYYY.YYYYYYYY.YYdddddh.hhhhmmmm.mmssssss.ffffffff.ffffffff.ffffffff
*/

/**
  Convert datetime to packed numeric datetime representation.
  @param ltime  The value to convert.
  @return       Packed numeric representation of ltime.
*/
longlong TIME_to_longlong_datetime_packed(const MYSQL_TIME *ltime) {
  longlong ymd = ((ltime->year * 13 + ltime->month) << 5) | ltime->day;
  longlong hms = (ltime->hour << 12) | (ltime->minute << 6) | ltime->second;
  longlong tmp = MY_PACKED_TIME_MAKE(((ymd << 17) | hms), ltime->second_part);
  DBUG_ASSERT(!check_datetime_range(ltime)); /* Make sure no overflow */
  return ltime->neg ? -tmp : tmp;
}

/**
  Convert date to packed numeric date representation.
  Numeric packed date format is similar to numeric packed datetime
  representation, with zero hhmmss part.

  @param ltime The value to convert.
  @return      Packed numeric representation of ltime.
*/
longlong TIME_to_longlong_date_packed(const MYSQL_TIME *ltime) {
  longlong ymd = ((ltime->year * 13 + ltime->month) << 5) | ltime->day;
  return MY_PACKED_TIME_MAKE_INT(ymd << 17);
}

/**
  Convert year to packed numeric date representation.
  Packed value for YYYY is the same to packed value for date YYYY-00-00.
*/
longlong year_to_longlong_datetime_packed(long year) {
  longlong ymd = ((year * 13) << 5);
  return MY_PACKED_TIME_MAKE_INT(ymd << 17);
}

/**
  Convert packed numeric datetime representation to MYSQL_TIME.
  @param [out] ltime The datetime variable to convert to.
  @param      tmp   The packed numeric datetime value.
*/
void TIME_from_longlong_datetime_packed(MYSQL_TIME *ltime, longlong tmp) {
  longlong ymd, hms;
  longlong ymdhms, ym;
  if ((ltime->neg = (tmp < 0))) tmp = -tmp;

  ltime->second_part = MY_PACKED_TIME_GET_FRAC_PART(tmp);
  ymdhms = MY_PACKED_TIME_GET_INT_PART(tmp);

  ymd = ymdhms >> 17;
  ym = ymd >> 5;
  hms = ymdhms % (1 << 17);

  ltime->day = ymd % (1 << 5);
  ltime->month = ym % 13;
  ltime->year = (uint)(ym / 13);

  ltime->second = hms % (1 << 6);
  ltime->minute = (hms >> 6) % (1 << 6);
  ltime->hour = (uint)(hms >> 12);

  ltime->time_type = MYSQL_TIMESTAMP_DATETIME;
}

/**
  Convert packed numeric date representation to MYSQL_TIME.
  @param [out] ltime The date variable to convert to.
  @param      tmp   The packed numeric date value.
*/
void TIME_from_longlong_date_packed(MYSQL_TIME *ltime, longlong tmp) {
  TIME_from_longlong_datetime_packed(ltime, tmp);
  ltime->time_type = MYSQL_TIMESTAMP_DATE;
}

/*
  On disk we store as unsigned number with DATETIMEF_INT_OFS offset,
  for HA_KETYPE_BINARY compatibilty purposes.
*/
#define DATETIMEF_INT_OFS 0x8000000000LL

/**
  Convert on-disk datetime representation
  to in-memory packed numeric representation.

  @param ptr   The pointer to read value at.
  @param dec   Precision.
  @return      In-memory packed numeric datetime representation.
*/
longlong my_datetime_packed_from_binary(const uchar *ptr, uint dec) {
  longlong intpart = mi_uint5korr(ptr) - DATETIMEF_INT_OFS;
  int frac;
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  switch (dec) {
    case 0:
    default:
      return MY_PACKED_TIME_MAKE_INT(intpart);
    case 1:
    case 2:
      frac = ((int)(signed char)ptr[5]) * 10000;
      break;
    case 3:
    case 4:
      frac = mi_sint2korr(ptr + 5) * 100;
      break;
    case 5:
    case 6:
      frac = mi_sint3korr(ptr + 5);
      break;
  }
  return MY_PACKED_TIME_MAKE(intpart, frac);
}

/**
  Store in-memory numeric packed datetime representation to disk.

  @param      nr  In-memory numeric packed datetime representation.
  @param [out] ptr The pointer to store at.
  @param      dec Precision, 1-6.
*/
void my_datetime_packed_to_binary(longlong nr, uchar *ptr, uint dec) {
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  /* The value being stored must have been properly rounded or truncated */
  DBUG_ASSERT((MY_PACKED_TIME_GET_FRAC_PART(nr) %
               (int)log_10_int[DATETIME_MAX_DECIMALS - dec]) == 0);

  mi_int5store(ptr, MY_PACKED_TIME_GET_INT_PART(nr) + DATETIMEF_INT_OFS);
  switch (dec) {
    case 0:
    default:
      break;
    case 1:
    case 2:
      ptr[5] = (unsigned char)(char)(MY_PACKED_TIME_GET_FRAC_PART(nr) / 10000);
      break;
    case 3:
    case 4:
      mi_int2store(ptr + 5, MY_PACKED_TIME_GET_FRAC_PART(nr) / 100);
      break;
    case 5:
    case 6:
      mi_int3store(ptr + 5, MY_PACKED_TIME_GET_FRAC_PART(nr));
  }
}

/*** TIMESTAMP low-level memory and disk representation routines ***/

/**
  Convert binary timestamp representation to in-memory representation.

  @param [out] tm  The variable to convert to.
  @param      ptr The pointer to read the value from.
  @param      dec Precision.
*/
void my_timestamp_from_binary(struct timeval *tm, const uchar *ptr, uint dec) {
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  tm->tv_sec = mi_uint4korr(ptr);
  switch (dec) {
    case 0:
    default:
      tm->tv_usec = 0;
      break;
    case 1:
    case 2:
      tm->tv_usec = ((int)ptr[4]) * 10000;
      break;
    case 3:
    case 4:
      tm->tv_usec = mi_sint2korr(ptr + 4) * 100;
      break;
    case 5:
    case 6:
      tm->tv_usec = mi_sint3korr(ptr + 4);
  }
}

/**
  Convert in-memory timestamp representation to on-disk representation.

  @param        tm   The value to convert.
  @param [out]  ptr  The pointer to store the value to.
  @param        dec  Precision.
*/
void my_timestamp_to_binary(const struct timeval *tm, uchar *ptr, uint dec) {
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  /* Stored value must have been previously properly rounded or truncated */
  DBUG_ASSERT((tm->tv_usec % (int)log_10_int[DATETIME_MAX_DECIMALS - dec]) ==
              0);
  mi_int4store(ptr, tm->tv_sec);
  switch (dec) {
    case 0:
    default:
      break;
    case 1:
    case 2:
      ptr[4] = (unsigned char)(char)(tm->tv_usec / 10000);
      break;
    case 3:
    case 4:
      mi_int2store(ptr + 4, tm->tv_usec / 100);
      break;
      /* Impossible second precision. Fall through */
    case 5:
    case 6:
      mi_int3store(ptr + 4, tm->tv_usec);
  }
}

/****************************************/

/**
  Convert a temporal value to packed numeric temporal representation,
  depending on its time_type.

  @param ltime   The value to convert.
  @return  Packed numeric time/date/datetime representation.
*/
longlong TIME_to_longlong_packed(const MYSQL_TIME *ltime) {
  switch (ltime->time_type) {
    case MYSQL_TIMESTAMP_DATE:
      return TIME_to_longlong_date_packed(ltime);
    case MYSQL_TIMESTAMP_DATETIME:
      return TIME_to_longlong_datetime_packed(ltime);
    case MYSQL_TIMESTAMP_TIME:
      return TIME_to_longlong_time_packed(ltime);
    case MYSQL_TIMESTAMP_NONE:
    case MYSQL_TIMESTAMP_ERROR:
      return 0;
  }
  DBUG_ASSERT(0);
  return 0;
}

/*** End of low level format routines ***/
