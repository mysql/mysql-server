<<<<<<< HEAD
/* Copyright (c) 2000, 2022, Oracle and/or its affiliates.
=======
<<<<<<< HEAD
/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.
=======
/* Copyright (c) 2000, 2023, Oracle and/or its affiliates.
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

/**
  @defgroup SQL_TIME Server time functions
  @ingroup Runtime_Environment
  @{

  @file sql/sql_time.cc

  Implementation of server functions to handle date and time.

*/

#include "sql/sql_time.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "decimal.h"
#include "m_ctype.h"
#include "m_string.h"
#include "my_compiler.h"

#include "my_macros.h"
#include "mysql_com.h"
#include "mysqld_error.h"
#include "sql/current_thd.h"
#include "sql/derror.h"
#include "sql/field.h"
#include "sql/my_decimal.h"
#include "sql/sql_class.h"  // THD, MODE_STRICT_ALL_TABLES, MODE_STRICT_TRANS_TABLES
#include "sql/sql_const.h"
#include "sql/system_variables.h"
#include "sql/tztime.h"  // struct Time_zone

/**
  Name description of interval names used in statements.

  'interval_type_to_name' is ordered and sorted on interval size and
  interval complexity.
  Order of elements in 'interval_type_to_name' should correspond to
  the order of elements in 'interval_type' enum

  @see interval_type, interval_names
*/
const LEX_CSTRING interval_type_to_name[INTERVAL_LAST] = {
    {STRING_WITH_LEN("YEAR")},
    {STRING_WITH_LEN("QUARTER")},
    {STRING_WITH_LEN("MONTH")},
    {STRING_WITH_LEN("WEEK")},
    {STRING_WITH_LEN("DAY")},
    {STRING_WITH_LEN("HOUR")},
    {STRING_WITH_LEN("MINUTE")},
    {STRING_WITH_LEN("SECOND")},
    {STRING_WITH_LEN("MICROSECOND")},
    {STRING_WITH_LEN("YEAR_MONTH")},
    {STRING_WITH_LEN("DAY_HOUR")},
    {STRING_WITH_LEN("DAY_MINUTE")},
    {STRING_WITH_LEN("DAY_SECOND")},
    {STRING_WITH_LEN("HOUR_MINUTE")},
    {STRING_WITH_LEN("HOUR_SECOND")},
    {STRING_WITH_LEN("MINUTE_SECOND")},
    {STRING_WITH_LEN("DAY_MICROSECOND")},
    {STRING_WITH_LEN("HOUR_MICROSECOND")},
    {STRING_WITH_LEN("MINUTE_MICROSECOND")},
    {STRING_WITH_LEN("SECOND_MICROSECOND")}};

/**
  Convert a string to 8-bit representation,
  for use in str_to_time/str_to_date/str_to_date.

  In the future to_ascii() can be extended to convert
  non-ASCII digits to ASCII digits
  (for example, ARABIC-INDIC, DEVANAGARI, BENGALI, and so on)
  so DATE/TIME/DATETIME values understand digits in the
  respected scripts.

  @return number of bytes written to dst
*/
static uint to_ascii(const CHARSET_INFO *cs, const char *src, size_t src_length,
                     char *dst, size_t dst_length) {
  int cnvres;
  my_wc_t wc;
  const char *srcend = src + src_length;
  char *dst0 = dst;
  char *dstend = dst + dst_length - 1;
  while (dst < dstend &&
         (cnvres = (cs->cset->mb_wc)(cs, &wc, pointer_cast<const uchar *>(src),
                                     pointer_cast<const uchar *>(srcend))) >
             0 &&
         wc < 128) {
    src += cnvres;
    *dst++ = static_cast<char>(wc);
  }
  *dst = '\0';
  return dst - dst0;
}

/**
   Character set-aware version of str_to_time().

   @return False on success, true on error.
*/
bool str_to_time(const CHARSET_INFO *cs, const char *str, size_t length,
                 MYSQL_TIME *l_time, my_time_flags_t flags,
                 MYSQL_TIME_STATUS *status) {
  char cnv[MAX_TIME_FULL_WIDTH + 3];  // +3 for nanoseconds (for rounding)
  if (!my_charset_is_ascii_based(cs)) {
    length = to_ascii(cs, str, length, cnv, sizeof(cnv));
    str = cnv;
  }

  bool rc = str_to_time(str, length, l_time, status);
  rc = rc || time_add_nanoseconds_adjust_frac(l_time, status->nanoseconds,
                                              &status->warnings,
                                              (flags & TIME_FRAC_TRUNCATE));
  return rc;
}

/**
   Character set-aware version of str_to_datetime().

   @return False on success, true on error.
*/
bool str_to_datetime(const CHARSET_INFO *cs, const char *str, size_t length,
                     MYSQL_TIME *l_time, my_time_flags_t flags,
                     MYSQL_TIME_STATUS *status) {
  char cnv[MAX_DATETIME_FULL_WIDTH + 3];  // +3 for nanoseconds (for rounding)
  if (!my_charset_is_ascii_based(cs)) {
    length = to_ascii(cs, str, length, cnv, sizeof(cnv));
    str = cnv;
  }

  bool rc = str_to_datetime(str, length, l_time, flags, status);
  rc = rc || datetime_add_nanoseconds_adjust_frac(l_time, status->nanoseconds,
                                                  &status->warnings,
                                                  flags & TIME_FRAC_TRUNCATE);
  return rc;
}

/**
<<<<<<< HEAD
=======
  @param [in,out] ltime        MYSQL_TIME variable to add to.
  @param          nanoseconds  Nanoseconds value.
  @param [in,out] warnings     Warning flag vector.
  @param          truncate     Decides whether fractional part of seconds will
                               be truncated/rounded.
  @retval                      False on success, true on error.
*/
bool datetime_add_nanoseconds_adjust_frac(MYSQL_TIME *ltime, uint nanoseconds,
                                          int *warnings, bool truncate) {
  if (truncate)
    return datetime_add_nanoseconds_with_truncate(ltime, nanoseconds);
  else
    return datetime_add_nanoseconds_with_round(ltime, nanoseconds, warnings);
}
#endif  // ifdef MYSQL_SERVER

/**
  @param [in,out] ltime        MYSQL_TIME variable to add to.
  @param          nanoseconds  Nanosecons value.
  @param [in,out] warnings     Warning flag vector.
  @param          truncate     Decides whether fractional part of seconds will
                               be truncated/rounded.
  @retval                      False on success. No real failure case here.
*/
bool time_add_nanoseconds_adjust_frac(MYSQL_TIME *ltime, uint nanoseconds,
                                      int *warnings, bool truncate) {
  if (truncate)
    return time_add_nanoseconds_with_truncate(ltime, nanoseconds, warnings);
  else
    return time_add_nanoseconds_with_round(ltime, nanoseconds, warnings);
}

/**
  If sql_mode is set for truncation then we just discard digits after
  fsp/DATETIME_MAX_DECIMALS.

  @param [in,out] ltime        MYSQL_TIME variable to add to.
  @param          nanoseconds  Nanoseconds value.
  @param [in,out] warnings     Warning flag vector.
  @retval                      False on success. No real failure case here.
*/
bool time_add_nanoseconds_with_truncate(MYSQL_TIME *ltime, uint nanoseconds,
                                        int *warnings) {
  /*
    If second_part is not set then only add nanoseconds to it.
    If second_part is already set and then nanoseconds just represent
    additional numbers which help rounding, so we can ignore them.
  */
  if (ltime->second_part == 0) ltime->second_part = nanoseconds / 1000;

  adjust_time_range(ltime, warnings);
  return false;
}

/**
  If sql_mode is set for truncation then we just discard digits after
  fsp/DATETIME_MAX_DECIMALS.

  @param [in,out] ltime        MYSQL_TIME variable to add to.
  @param          nanoseconds  Nanoseconds value.
  @retval                      False on success. No real failure case here.
*/
bool datetime_add_nanoseconds_with_truncate(MYSQL_TIME *ltime,
                                            uint nanoseconds) {
  /*
    If second_part is not set then only add nanoseconds to it.
    If second_part is already set and then nanoseconds just represent
    additional numbers which help rounding, so we can ignore them.
  */
  if (ltime->second_part == 0) ltime->second_part = nanoseconds / 1000;
  return false;
}

/**
  Add nanoseconds to a time value with rounding.

  @param [in,out] ltime        MYSQL_TIME variable to add to.
  @param          nanoseconds  Nanoseconds value.
  @param [in,out] warnings     Warning flag vector.
  @retval                      False on success, true on error.
*/
bool time_add_nanoseconds_with_round(MYSQL_TIME *ltime, uint nanoseconds,
                                     int *warnings) {
  /* We expect correct input data */
  assert(nanoseconds < 1000000000);
  assert(!check_time_mmssff_range(ltime));

  if (nanoseconds < 500) return false;

  ltime->second_part += (nanoseconds + 500) / 1000;
  if (ltime->second_part < 1000000) goto ret;

  ltime->second_part %= 1000000;
  if (ltime->second < 59) {
    ltime->second++;
    goto ret;
  }

  ltime->second = 0;
  if (ltime->minute < 59) {
    ltime->minute++;
    goto ret;
  }
  ltime->minute = 0;
  ltime->hour++;

ret:
  /*
    We can get '838:59:59.000001' at this point, which
    is bigger than the maximum possible value '838:59:59.000000'.
    Checking only "hour > 838" is not enough.
    Do full adjust_time_range().
  */
  adjust_time_range(ltime, warnings);
  return false;
}

/**
  Add nanoseconds to a datetime value with rounding.

  @param [in,out] ltime        MYSQL_TIME variable to add to.
  @param          nanoseconds  Nanoseconds value.
  @param [in,out] warnings     Warning flag vector.
  @retval                      False on success, true on error.
*/
<<<<<<< HEAD
#ifdef MYSQL_SERVER
bool datetime_add_nanoseconds_with_round(MYSQL_TIME *ltime, uint nanoseconds,
                                         int *warnings) {
  DBUG_ASSERT(nanoseconds < 1000000000);
  if (nanoseconds < 500) return false;
=======
bool datetime_add_nanoseconds_with_round(MYSQL_TIME *ltime,
                                         uint nanoseconds, int *warnings)
{
  assert(nanoseconds < 1000000000);
  if (nanoseconds < 500)
    return false;
>>>>>>> upstream/cluster-7.6

  ltime->second_part += (nanoseconds + 500) / 1000;
  if (ltime->second_part < 1000000) return false;

  ltime->second_part %= 1000000;
  Interval interval;
  memset(&interval, 0, sizeof(interval));
  interval.second = 1;
  /* date_add_interval cannot handle bad dates */
  if (check_date(ltime, non_zero_date(ltime),
                 (TIME_NO_ZERO_IN_DATE | TIME_NO_ZERO_DATE), warnings))
    return true;

  if (date_add_interval(ltime, INTERVAL_SECOND, interval)) {
    *warnings |= MYSQL_TIME_WARN_OUT_OF_RANGE;
    return true;
  }
  return false;
}

ulonglong TIME_to_ulonglong_datetime_round(const MYSQL_TIME *ltime) {
  // Catch simple cases
  if (ltime->second_part < 500000) return TIME_to_ulonglong_datetime(ltime);
  if (ltime->second < 59) return TIME_to_ulonglong_datetime(ltime) + 1;
  // Corner case e.g. 'YYYY-MM-DD hh:mm:59.5'. Proceed with slower method.
  int warnings = 0;
  MYSQL_TIME tmp = *ltime;
  my_datetime_adjust_frac(&tmp, 0, &warnings, false);
  return TIME_to_ulonglong_datetime(&tmp);  // + TIME_microseconds_round(ltime);
}

ulonglong TIME_to_ulonglong_time_round(const MYSQL_TIME *ltime) {
  if (ltime->second_part < 500000) return TIME_to_ulonglong_time(ltime);
  if (ltime->second < 59) return TIME_to_ulonglong_time(ltime) + 1;
  // Corner case e.g. 'hh:mm:59.5'. Proceed with slower method.
  MYSQL_TIME tmp = *ltime;
  my_time_adjust_frac(&tmp, 0, false);
  return TIME_to_ulonglong_time(&tmp);
}

/*
>>>>>>> pr/231
  Convert a timestamp string to a MYSQL_TIME value and produce a warning
  if string was truncated during conversion.

  @note See description of str_to_datetime() for more information.
  @note Uses current_thd

  @return False on success, true on error.
*/
bool str_to_datetime_with_warn(String *str, MYSQL_TIME *l_time,
                               my_time_flags_t flags) {
  MYSQL_TIME_STATUS status;
  THD *thd = current_thd;
  if (thd->variables.sql_mode & MODE_NO_ZERO_DATE) flags |= TIME_NO_ZERO_DATE;
  if (thd->variables.sql_mode & MODE_INVALID_DATES) flags |= TIME_INVALID_DATES;
  if (thd->is_fsp_truncate_mode()) flags |= TIME_FRAC_TRUNCATE;
  bool ret_val = propagate_datetime_overflow(
      thd, &status.warnings, str_to_datetime(str, l_time, flags, &status));
  if (ret_val || status.warnings) {
    if (make_truncated_value_warning(current_thd, Sql_condition::SL_WARNING,
                                     ErrConvString(str), l_time->time_type,
                                     NullS))
      return true;
    if (ret_val) status.squelch_deprecation();
  }
  check_deprecated_datetime_format(current_thd, str->charset(), status);

  if (ret_val) return true;
  return convert_time_zone_displacement(thd->time_zone(), l_time);
}

/**
  Convert lldiv_t to datetime.

  @param         lld      The value to convert from.
  @param[out]    ltime    The variable to convert to.
  @param         flags    Conversion flags.
  @param[in,out] warnings Warning flags.

  @return False on success, true on error.
*/
static bool lldiv_t_to_datetime(lldiv_t lld, MYSQL_TIME *ltime,
                                my_time_flags_t flags, int *warnings) {
  if (lld.rem < 0 ||  // Catch negative numbers with zero int part, e.g: -0.1
      number_to_datetime(lld.quot, ltime, flags, warnings) == -1LL) {
    /* number_to_datetime does not clear ltime in case of ZERO DATE */
    set_zero_time(ltime, MYSQL_TIMESTAMP_ERROR);
    if (!*warnings) /* Neither sets warnings in case of ZERO DATE */
      *warnings |= MYSQL_TIME_WARN_TRUNCATED;
    return true;
  }
  if (ltime->time_type == MYSQL_TIMESTAMP_DATE) {
    /*
      Generate a warning in case of DATE with fractional part:
        20011231.1234 -> '2001-12-31'
      unless the caller does not want the warning: for example, CAST does.
    */
    if (lld.rem && !(flags & TIME_NO_DATE_FRAC_WARN))
      *warnings |= MYSQL_TIME_WARN_TRUNCATED;
  } else {
    ltime->second_part = static_cast<ulong>(lld.rem / 1000);
    return datetime_add_nanoseconds_adjust_frac(ltime, lld.rem % 1000, warnings,
                                                (flags & TIME_FRAC_TRUNCATE));
  }
  return false;
}

/**
  Convert decimal value to datetime.

  @param       decimal The value to convert from.
  @param[out]  ltime   The variable to convert to.
  @param       flags   Conversion flags.

  @returns false on success, true if not convertible to datetime.
*/
bool decimal_to_datetime(const my_decimal *decimal, MYSQL_TIME *ltime,
                         my_time_flags_t flags) {
  lldiv_t lld;
  if (my_decimal2lldiv_t(0, decimal, &lld)) {
    return true;
  }
  int warnings = 0;
  return lldiv_t_to_datetime(lld, ltime, flags, &warnings);
}

/**
  Convert decimal value to datetime value with a warning.
  @param       decimal The value to convert from.
  @param[out]  ltime   The variable to convert to.
  @param       flags   Conversion flags.

  @return False on success, true on error.
*/
bool my_decimal_to_datetime_with_warn(const my_decimal *decimal,
                                      MYSQL_TIME *ltime,
                                      my_time_flags_t flags) {
  lldiv_t lld;
  int warnings = 0;
  bool rc;

  if ((rc = my_decimal2lldiv_t(0, decimal, &lld))) {
    warnings |= MYSQL_TIME_WARN_TRUNCATED;
    set_zero_time(ltime, MYSQL_TIMESTAMP_NONE);
  } else
    rc = propagate_datetime_overflow(
        current_thd, &warnings,
        lldiv_t_to_datetime(lld, ltime, flags, &warnings));

  if (warnings) {
    if (make_truncated_value_warning(current_thd, Sql_condition::SL_WARNING,
                                     ErrConvString(decimal), ltime->time_type,
                                     NullS))
      return true;
  }
  return rc;
}

/**
  Convert double value to datetime.

  @param       nr      The value to convert from.
  @param[out]  ltime   The variable to convert to.
  @param       flags   Conversion flags.

  @returns false on success, true if not convertible to datetime.
*/
bool double_to_datetime(double nr, MYSQL_TIME *ltime, my_time_flags_t flags) {
  lldiv_t lld;
  if (double2lldiv_t(nr, &lld)) {
    return true;
  }
  int warnings = 0;
  return lldiv_t_to_datetime(lld, ltime, flags, &warnings);
}

/**
  Convert double value to datetime value with a warning.
  @param       nr      The value to convert from.
  @param[out]  ltime   The variable to convert to.
  @param       flags   Conversion flags.

  @return False on success, true on error.
*/
bool my_double_to_datetime_with_warn(double nr, MYSQL_TIME *ltime,
                                     my_time_flags_t flags) {
  lldiv_t lld;
  int warnings = 0;
  bool rc;

  if ((rc = (double2lldiv_t(nr, &lld) != E_DEC_OK))) {
    warnings |= MYSQL_TIME_WARN_TRUNCATED;
    set_zero_time(ltime, MYSQL_TIMESTAMP_NONE);
  } else
    rc = propagate_datetime_overflow(
        current_thd, &warnings,
        lldiv_t_to_datetime(lld, ltime, flags, &warnings));

  if (warnings) {
    if (make_truncated_value_warning(current_thd, Sql_condition::SL_WARNING,
                                     ErrConvString(nr), ltime->time_type,
                                     NullS))
      return true;
  }
  return rc;
}

/**
  Convert longlong value to datetime value with a warning.
  @param       nr      The value to convert from.
  @param[out]  ltime   The variable to convert to.
  @param       flags   Conversion flags

  @return False on success, true on error.
*/
bool my_longlong_to_datetime_with_warn(longlong nr, MYSQL_TIME *ltime,
                                       my_time_flags_t flags) {
  int warnings = 0;
  bool rc = propagate_datetime_overflow(
                current_thd, &warnings,
                number_to_datetime(nr, ltime, flags, &warnings)) == -1LL;
  if (warnings) {
    if (make_truncated_value_warning(current_thd, Sql_condition::SL_WARNING,
                                     ErrConvString(nr), MYSQL_TIMESTAMP_NONE,
                                     NullS))
      return true;
  }
  return rc;
}

/**
  Convert lldiv_t value to time with nanosecond rounding.

  @param         lld      The value to convert from.
  @param[out]    ltime    The variable to convert to,
  @param[in,out] warnings Warning flags.

  @return False on success, true on error.
*/
static bool lldiv_t_to_time(lldiv_t lld, MYSQL_TIME *ltime, int *warnings) {
  if (number_to_time(lld.quot, ltime, warnings)) return true;
  /*
    Both lld.quot and lld.rem can give negative result value,
    thus combine them using "|=".
  */
  if ((ltime->neg |= (lld.rem < 0))) lld.rem = -lld.rem;
  ltime->second_part = static_cast<ulong>(lld.rem / 1000);
  return time_add_nanoseconds_adjust_frac(ltime, lld.rem % 1000, warnings,
                                          current_thd->is_fsp_truncate_mode());
}

/**
  Convert decimal number to TIME

  @param      decimal  The number to convert from.
  @param[out] ltime          The variable to convert to.

  @returns false on success, true if not convertible to time.
*/
bool decimal_to_time(const my_decimal *decimal, MYSQL_TIME *ltime) {
  lldiv_t lld;
  int warnings = 0;

  if (my_decimal2lldiv_t(0, decimal, &lld)) {
    return true;
  }
  return lldiv_t_to_time(lld, ltime, &warnings);
}

/**
  Convert decimal number to TIME
  @param      decimal  The number to convert from.
  @param[out] ltime          The variable to convert to.

  @return False on success, true on error.
*/
bool my_decimal_to_time_with_warn(const my_decimal *decimal,
                                  MYSQL_TIME *ltime) {
  lldiv_t lld;
  int warnings = 0;
  bool rc;

  if ((rc = my_decimal2lldiv_t(0, decimal, &lld))) {
    warnings |= MYSQL_TIME_WARN_TRUNCATED;
    set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);
  } else
    rc = propagate_datetime_overflow(current_thd, &warnings,
                                     lldiv_t_to_time(lld, ltime, &warnings));

  if (warnings) {
    if (make_truncated_value_warning(current_thd, Sql_condition::SL_WARNING,
                                     ErrConvString(decimal),
                                     MYSQL_TIMESTAMP_TIME, NullS))
      return true;
  }
  return rc;
}

/**
  Convert double number to TIME

  @param      nr      The number to convert from.
  @param[out] ltime   The variable to convert to.

  @returns false on success, true if not convertible to time.
*/
bool double_to_time(double nr, MYSQL_TIME *ltime) {
  lldiv_t lld;
  if (double2lldiv_t(nr, &lld) != E_DEC_OK) {
    return true;
  }
  int warnings = 0;
  return lldiv_t_to_time(lld, ltime, &warnings);
}

/**
  Convert double number to TIME

  @param      nr      The number to convert from.
  @param[out] ltime   The variable to convert to.

  @return False on success, true on error.
*/
bool my_double_to_time_with_warn(double nr, MYSQL_TIME *ltime) {
  lldiv_t lld;
  int warnings = 0;
  bool rc;

  if ((rc = (double2lldiv_t(nr, &lld) != E_DEC_OK))) {
    warnings |= MYSQL_TIME_WARN_TRUNCATED;
    set_zero_time(ltime, MYSQL_TIMESTAMP_TIME);
  } else
    rc = propagate_datetime_overflow(current_thd, &warnings,
                                     lldiv_t_to_time(lld, ltime, &warnings));

  if (warnings) {
    if (make_truncated_value_warning(current_thd, Sql_condition::SL_WARNING,
                                     ErrConvString(nr), MYSQL_TIMESTAMP_TIME,
                                     NullS))
      return true;
  }
  return rc;
}

/**
  Convert longlong number to TIME
  @param      nr     The number to convert from.
  @param[out] ltime  The variable to convert to.

  @return False on success, true on error.
*/
bool my_longlong_to_time_with_warn(longlong nr, MYSQL_TIME *ltime) {
  int warnings = 0;
  bool rc = propagate_datetime_overflow(current_thd, &warnings,
                                        number_to_time(nr, ltime, &warnings));
  if (warnings) {
    if (make_truncated_value_warning(current_thd, Sql_condition::SL_WARNING,
                                     ErrConvString(nr), MYSQL_TIMESTAMP_TIME,
                                     NullS))
      return true;
  }
  return rc;
}

/**
  Converts a datetime in MYSQL_TIME representation to corresponding `struct
  timeval` value.

  `ltime` must be previously checked for `TIME_NO_ZERO_IN_DATE`.
  Things like '0000-01-01', '2000-00-01', '2000-01-00' are not allowed
  and asserted.

  Things like '0000-00-00 10:30:30' or '0000-00-00 00:00:00.123456'
  (i.e. empty date with non-empty time) return error.

  Zero datetime '0000-00-00 00:00:00.000000' is allowed and is mapped to
  {tv_sec=0, tv_usec=0}.

  @note In case of error, tm value is not initialized.

  @note `warnings` is not initialized to zero, so new warnings are added to the
  old ones. The caller must make sure to initialize `warnings`.

  @param[in]  ltime    Datetime value
  @param[in]  tz       Time zone to convert to.
  @param[out] tm       Timeval value
  @param[out] warnings Pointer to warnings.

  @return False on success, true on error.
*/
bool datetime_with_no_zero_in_date_to_timeval(const MYSQL_TIME *ltime,
                                              const Time_zone &tz,
                                              my_timeval *tm, int *warnings) {
  if (!ltime->month) /* Zero date */
  {
<<<<<<< HEAD
    assert(!ltime->year && !ltime->day);
    if (non_zero_time(*ltime)) {
=======
<<<<<<< HEAD
    DBUG_ASSERT(!ltime->year && !ltime->day);
    if (non_zero_time(ltime)) {
=======
    assert(!ltime->year && !ltime->day);
    if (non_zero_time(ltime))
    {
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
      /*
        Return error for zero date with non-zero time, e.g.:
        '0000-00-00 10:20:30' or '0000-00-00 00:00:00.123456'
      */
      *warnings |= MYSQL_TIME_WARN_TRUNCATED;
      return true;
    }
    tm->m_tv_sec = tm->m_tv_usec = 0;  // '0000-00-00 00:00:00.000000'
    return false;
  }

  bool is_in_dst_time_gap = false;
  if (!(tm->m_tv_sec = tz.TIME_to_gmt_sec(ltime, &is_in_dst_time_gap))) {
    /*
      Date was outside of the supported timestamp range.
      For example: '3001-01-01 00:00:00' or '1000-01-01 00:00:00'
    */
    *warnings |= MYSQL_TIME_WARN_OUT_OF_RANGE;
    return true;
  }
  if (is_in_dst_time_gap) {
    /*
      Set MYSQL_TIME_WARN_INVALID_TIMESTAMP warning to indicate
      that date was fine but pointed to winter/summer time switch gap.
      In this case tm is set to the fist second after gap.
      For example: '2003-03-30 02:30:00 MSK' -> '2003-03-30 03:00:00 MSK'
    */
    *warnings |= MYSQL_TIME_WARN_INVALID_TIMESTAMP;
  }
  tm->m_tv_usec = ltime->second_part;
  return false;
}

/**
  Convert a datetime MYSQL_TIME representation
  to corresponding "struct timeval" value.

  Things like '0000-01-01', '2000-00-01', '2000-01-00'
  (i.e. incomplete date) return error.

  Things like '0000-00-00 10:30:30' or '0000-00-00 00:00:00.123456'
  (i.e. empty date with non-empty time) return error.

  Zero datetime '0000-00-00 00:00:00.000000'
  is allowed and is mapper to {tv_sec=0, tv_usec=0}.

  Note: In case of error, tm value is not initialized.

  Note: "warnings" is not initialized to zero,
  so new warnings are added to the old ones.
  Caller must make sure to initialize "warnings".

  @param[in]  ltime     datetime value
  @param[in]  tz       The time zone.
  @param[out] tm        timeval value
  @param[out] warnings  pointer to warnings vector

  @return False on success, true on error.
*/
bool datetime_to_timeval(const MYSQL_TIME *ltime, const Time_zone &tz,
                         my_timeval *tm, int *warnings) {
  return check_date(*ltime, non_zero_date(*ltime), TIME_NO_ZERO_IN_DATE,
                    warnings) ||
         datetime_with_no_zero_in_date_to_timeval(ltime, tz, tm, warnings);
}

/**
  Convert a time string to a MYSQL_TIME struct and produce a warning
  if string was cut during conversion.

  @note See str_to_time() for more info.

  @return False on success, true on error.
*/
bool str_to_time_with_warn(String *str, MYSQL_TIME *l_time) {
  MYSQL_TIME_STATUS status;
  my_time_flags_t flags = 0;
  THD *thd = current_thd;

  if (current_thd->is_fsp_truncate_mode()) flags = TIME_FRAC_TRUNCATE;

  bool ret_val = propagate_datetime_overflow(
      current_thd, &status.warnings, str_to_time(str, l_time, flags, &status));
  if (ret_val || status.warnings) {
    if (make_truncated_value_warning(thd, Sql_condition::SL_WARNING,
                                     ErrConvString(str), MYSQL_TIMESTAMP_TIME,
                                     NullS))
      return true;
  }
  check_deprecated_datetime_format(current_thd, str->charset(), status);
  if (!ret_val)
    if (convert_time_zone_displacement(thd->time_zone(), l_time)) return true;

  return ret_val;
}

/**
  Convert time to datetime.

  The time value is added to the current datetime value.
  @param thd            Thread context
  @param [in] ltime     Time value to convert from.
  @param [out] ltime2   Datetime value to convert to.
*/
void time_to_datetime(THD *thd, const MYSQL_TIME *ltime, MYSQL_TIME *ltime2) {
  thd->variables.time_zone->gmt_sec_to_TIME(
      ltime2, static_cast<my_time_t>(thd->query_start_in_secs()));
  ltime2->hour = ltime2->minute = ltime2->second = ltime2->second_part = 0;
  ltime2->time_type = MYSQL_TIMESTAMP_DATE;
<<<<<<< HEAD
  mix_date_and_time(ltime2, *ltime);
}

/**
=======
  mix_date_and_time(ltime2, ltime);
}
#endif  // ifdef MYSQL_SERVER

/*
  Convert a system time structure to TIME
*/

void localtime_to_TIME(MYSQL_TIME *to, struct tm *from) {
  to->neg = 0;
  to->second_part = 0;
  to->year = (int)((from->tm_year + 1900) % 10000);
  to->month = (int)from->tm_mon + 1;
  to->day = (int)from->tm_mday;
  to->hour = (int)from->tm_hour;
  to->minute = (int)from->tm_min;
  to->second = (int)from->tm_sec;
}

void calc_time_from_sec(MYSQL_TIME *to, longlong seconds, long microseconds) {
  long t_seconds;
  // to->neg is not cleared, it may already be set to a useful value
<<<<<<< HEAD
  to->time_type = MYSQL_TIMESTAMP_TIME;
  to->year = 0;
  to->month = 0;
  to->day = 0;
  DBUG_ASSERT(seconds < (0xFFFFFFFFLL * 3600LL));
  to->hour = (long)(seconds / 3600L);
  t_seconds = (long)(seconds % 3600L);
  to->minute = t_seconds / 60L;
  to->second = t_seconds % 60L;
  to->second_part = microseconds;
=======
  to->time_type= MYSQL_TIMESTAMP_TIME;
  to->year= 0;
  to->month= 0;
  to->day= 0;
  assert(seconds < (0xFFFFFFFFLL * 3600LL));
  to->hour=  (long) (seconds / 3600L);
  t_seconds= (long) (seconds % 3600L);
  to->minute= t_seconds/60L;
  to->second= t_seconds%60L;
  to->second_part= microseconds;
>>>>>>> upstream/cluster-7.6
}

/**
  Parse a format string specification

  @param format_type  Format of string (time, date or datetime)
  @param date_time_format   Format to fill in

  Fills in date_time_format->positions for all date time parts.

  positions marks the position for a datetime element in the format string.
  The position array elements are in the following order:
  YYYY-DD-MM HH-MM-DD.FFFFFF AM
  0    1  2  3  4  5  6      7

  If positions[0]= 5, it means that year will be the forth element to
  read from the parsed date string.

  @return true if error, false otherwise.
*/

bool parse_date_time_format(timestamp_type format_type,
                            Date_time_format *date_time_format) {
  const char *format = date_time_format->format.str;
  size_t format_length = date_time_format->format.length;
  uint offset = 0, separators = 0;
  const char *ptr = format, *format_str;
  const char *end = ptr + format_length;
  uchar *dt_pos = date_time_format->positions;
  /* need_p is set if we are using AM/PM format */
  bool need_p = 0, allow_separator = 0;
  ulong part_map = 0, separator_map = 0;
  const char *parts[16];

  /*
    Fill position with 'dummy' arguments to found out if a format tag is
    used twice (This limit's the format to 255 characters, but this is ok)
  */
  dt_pos[0] = dt_pos[1] = dt_pos[2] = dt_pos[3] = dt_pos[4] = dt_pos[5] =
      dt_pos[6] = dt_pos[7] = 255;

  for (; ptr != end; ptr++) {
    if (*ptr == '%' && ptr + 1 != end) {
      uint position = 0;
      switch (*++ptr) {
        case 'y':  // Year
        case 'Y':
          position = 0;
          break;
        case 'c':  // Month
        case 'm':
          position = 1;
          break;
        case 'd':
        case 'e':
          position = 2;
          break;
        case 'h':
        case 'I':
        case 'l':
          need_p = 1;  // Need AM/PM
                       /* Fall through */
        case 'k':
        case 'H':
          position = 3;
          break;
        case 'i':
          position = 4;
          break;
        case 's':
        case 'S':
          position = 5;
          break;
        case 'f':
          position = 6;
          if (dt_pos[5] != offset - 1 || ptr[-2] != '.')
            return 1;  // Wrong usage of %f
          break;
        case 'p':           // AM/PM
          if (offset == 0)  // Can't be first
            return 0;
          position = 7;
          break;
        default:
          return 1;  // Unknown controll char
      }
      if (dt_pos[position] != 255)  // Don't allow same tag twice
        return 1;
      parts[position] = ptr - 1;

      /*
        If switching from time to date, ensure that all time parts
        are used
      */
      if (part_map && position <= 2 && !(part_map & (1 | 2 | 4))) offset = 5;
      part_map |= (ulong)1 << position;
      dt_pos[position] = offset++;
      allow_separator = 1;
    } else {
      /*
        Don't allow any characters in format as this could easily confuse
        the date reader
      */
      if (!allow_separator) return 1;  // No separator here
      allow_separator = 0;             // Don't allow two separators
      separators++;
      /* Store in separator_map which parts are punct characters */
      if (my_ispunct(&my_charset_latin1, *ptr))
        separator_map |= (ulong)1 << (offset - 1);
      else if (!my_isspace(&my_charset_latin1, *ptr))
        return 1;
    }
  }

  /* If no %f, specify it after seconds.  Move %p up, if necessary */
  if ((part_map & 32) && !(part_map & 64)) {
    dt_pos[6] = dt_pos[5] + 1;
    parts[6] = parts[5];         // For later test in (need_p)
    if (dt_pos[6] == dt_pos[7])  // Move %p one step up if used
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
      !allow_separator ||  // %option should be last
      (need_p && dt_pos[6] + 1 != dt_pos[7]) || (need_p ^ (dt_pos[7] != 255)))
    return 1;

  if (dt_pos[6] != 255)  // If fractional seconds
  {
    /* remove fractional seconds from later tests */
    uint pos = dt_pos[6] - 1;
    /* Remove separator before %f from sep map */
    separator_map = ((separator_map & ((ulong)(1 << pos) - 1)) |
                     ((separator_map & ~((ulong)(1 << pos) - 1)) >> 1));
    if (part_map & 64) {
      separators--;  // There is always a separator
      need_p = 1;    // force use of separators
    }
  }

  /*
    Remove possible separator before %p from sep_map
    (This can either be at position 3, 4, 6 or 7) h.m.d.%f %p
  */
  if (dt_pos[7] != 255) {
    if (need_p && parts[7] != parts[6] + 2) separators--;
  }
  /*
    Calculate if %p is in first or last part of the datetime field

    At this point we have either %H-%i-%s %p 'year parts' or
    'year parts' &H-%i-%s %p" as %f was removed above
  */
  offset = dt_pos[6] <= 3 ? 3 : 6;
  /* Remove separator before %p from sep map */
  separator_map = ((separator_map & ((ulong)(1 << offset) - 1)) |
                   ((separator_map & ~((ulong)(1 << offset) - 1)) >> 1));

  format_str = 0;
  switch (format_type) {
    case MYSQL_TIMESTAMP_DATE:
      format_str = known_date_time_formats[INTERNAL_FORMAT].date_format;
      /* fall through */
    case MYSQL_TIMESTAMP_TIME:
      if (!format_str)
        format_str = known_date_time_formats[INTERNAL_FORMAT].time_format;

      /*
        If there is no separators, allow the internal format as we can read
        this.  If separators are used, they must be between each part
      */
      if (format_length == 6 && !need_p &&
          !my_strnncoll(&my_charset_bin, (const uchar *)format, 6,
                        (const uchar *)format_str, 6))
        return 0;
      if (separator_map == (1 | 2)) {
        if (format_type == MYSQL_TIMESTAMP_TIME) {
          if (*(format + 2) != *(format + 5)) break;  // Error
        }
        return 0;
      }
<<<<<<< HEAD
      break;
    case MYSQL_TIMESTAMP_DATETIME:
      /*
        If there is no separators, allow the internal format as we can read
        this.  If separators are used, they must be between each part.
        Between DATE and TIME we also allow space as separator
      */
      if ((format_length == 12 && !need_p &&
           !my_strnncoll(&my_charset_bin, (const uchar *)format, 12,
                         (const uchar *)known_date_time_formats[INTERNAL_FORMAT]
                             .datetime_format,
                         12)) ||
          (separators == 5 && separator_map == (1 | 2 | 8 | 16)))
        return 0;
      break;
    default:
      DBUG_ASSERT(false);
      break;
=======
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
    assert(1);
    break;
>>>>>>> upstream/cluster-7.6
  }
  return 1;  // Error
}

Known_date_time_format known_date_time_formats[6] = {
    {"USA", "%m.%d.%Y", "%Y-%m-%d %H.%i.%s", "%h:%i:%s %p"},
    {"JIS", "%Y-%m-%d", "%Y-%m-%d %H:%i:%s", "%H:%i:%s"},
    {"ISO", "%Y-%m-%d", "%Y-%m-%d %H:%i:%s", "%H:%i:%s"},
    {"EUR", "%d.%m.%Y", "%Y-%m-%d %H.%i.%s", "%H.%i.%s"},
    {"INTERNAL", "%Y%m%d", "%Y%m%d%H%i%s", "%H%i%s"},
    {0, 0, 0, 0}};

/*
>>>>>>> pr/231
   Return format string according format name.
   If name is unknown, result is NULL

   @returns format string according format name.
   @retval NULL if name is unknown.
*/
const char *get_date_time_format_str(const Known_date_time_format *format,
                                     enum_mysql_timestamp_type type) {
  switch (type) {
<<<<<<< HEAD
    case MYSQL_TIMESTAMP_DATE:
      return format->date_format;
    case MYSQL_TIMESTAMP_DATETIME:
      return format->datetime_format;
    case MYSQL_TIMESTAMP_TIME:
      return format->time_format;
    default:
<<<<<<< HEAD
      assert(0);  // Impossible
      return nullptr;
=======
      DBUG_ASSERT(0);  // Impossible
      return 0;
=======
  case MYSQL_TIMESTAMP_DATE:
    return format->date_format;
  case MYSQL_TIMESTAMP_DATETIME:
    return format->datetime_format;
  case MYSQL_TIMESTAMP_TIME:
    return format->time_format;
  default:
    assert(0);				// Impossible
    return 0;
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
}

/**
   @ingroup SQL_TIME
   @page DEFAULT_TIME_FUNCS Functions to create default time/date/datetime
   strings
   @note
    For the moment the Date_time_format argument is ignored because
    MySQL doesn't support comparing of date/time/datetime strings that
    are not in arbutary order as dates are compared as strings in some
    context)
    This functions don't check that given MYSQL_TIME structure members are
    in valid range. If they are not, return value won't reflect any
    valid date either. Additionally, make_time doesn't take into
    account time->day member: it's assumed that days have been converted
    to hours already.
*/

/**
  Convert TIME value to String.
  @param      format   Format (unused, see comments above)
  @param      l_time   TIME value
  @param[out] str      String to convert to
  @param      dec      Number of fractional digits.
*/
void make_time(const Date_time_format *format [[maybe_unused]],
               const MYSQL_TIME *l_time, String *str, uint dec) {
  uint length = static_cast<uint>(my_time_to_str(*l_time, str->ptr(), dec));
  str->length(length);
  str->set_charset(&my_charset_numeric);
}

/**
  Convert DATE value to String.
  @param      format   Format (unused, see comments above)
  @param      l_time   DATE value
  @param[out] str      String to convert to
*/
void make_date(const Date_time_format *format [[maybe_unused]],
               const MYSQL_TIME *l_time, String *str) {
  uint length = static_cast<uint>(my_date_to_str(*l_time, str->ptr()));
  str->length(length);
  str->set_charset(&my_charset_numeric);
}

/**
  Convert DATETIME value to String.
  @param      format   Format (unused, see comments above)
  @param      l_time   DATE value
  @param[out] str      String to convert to
  @param      dec      Number of fractional digits.
*/
void make_datetime(const Date_time_format *format [[maybe_unused]],
                   const MYSQL_TIME *l_time, String *str, uint dec) {
  uint length = static_cast<uint>(my_datetime_to_str(*l_time, str->ptr(), dec));
  str->length(length);
  str->set_charset(&my_charset_numeric);
}

/**
  Convert TIME/DATE/DATETIME value to String.
  @param      ltime    DATE value
  @param[out] str      String to convert to
  @param      dec      Number of fractional digits.
*/
bool my_TIME_to_str(const MYSQL_TIME *ltime, String *str, uint dec) {
  if (str->alloc(MAX_DATE_STRING_REP_LENGTH)) return true;
  str->set_charset(&my_charset_numeric);
  str->length(my_TIME_to_str(*ltime, str->ptr(), dec));
  return false;
}

/**
   Create and add a truncated value warning to the THD.

   @returns value of thd->is_error() after adding the warning
 */
bool make_truncated_value_warning(THD *thd,
                                  Sql_condition::enum_severity_level level,
                                  const ErrConvString &val,
                                  enum_mysql_timestamp_type time_type,
                                  const char *field_name) {
  char warn_buff[MYSQL_ERRMSG_SIZE];
  const char *type_str;
  CHARSET_INFO *cs = system_charset_info;

  switch (time_type) {
    case MYSQL_TIMESTAMP_DATE:
      type_str = "date";
      break;
    case MYSQL_TIMESTAMP_TIME:
      type_str = "time";
      break;
    case MYSQL_TIMESTAMP_DATETIME:  // FALLTHROUGH
    default:
      type_str = "datetime";
      break;
  }
  if (field_name)
    cs->cset->snprintf(
        cs, warn_buff, sizeof(warn_buff),
        ER_THD(thd, ER_TRUNCATED_WRONG_VALUE_FOR_FIELD), type_str, val.ptr(),
        field_name,
        static_cast<long>(thd->get_stmt_da()->current_row_for_condition()));
  else {
    if (time_type > MYSQL_TIMESTAMP_ERROR)
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER_THD(thd, ER_TRUNCATED_WRONG_VALUE), type_str,
                         val.ptr());
    else
      cs->cset->snprintf(cs, warn_buff, sizeof(warn_buff),
                         ER_THD(thd, ER_WRONG_VALUE), type_str, val.ptr());
  }
  push_warning(thd, level, ER_TRUNCATED_WRONG_VALUE, warn_buff);

  // strict mode can convert warning to error. Check for error while returning.
  return thd->is_error();
}

/**
   Uses propagate_datetime_overflow() to handle and propagate any warnings from
   date_add_interval() to the THD.

<<<<<<< HEAD
   @return False on success, true on error.
 */
bool date_add_interval_with_warn(THD *thd, MYSQL_TIME *ltime,
                                 interval_type int_type, Interval interval) {
  return propagate_datetime_overflow(thd, [&](int *w) {
    return date_add_interval(ltime, int_type, interval, w);
  });
=======
  @param [in,out]  ltime    The value to truncate.
  @param           dec      Precision.
  @return                   False on success, true on error.
*/
<<<<<<< HEAD
bool my_time_truncate(MYSQL_TIME *ltime, uint dec) {
  int warnings = 0;
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  bool rc =
      time_add_nanoseconds_with_truncate(ltime, msec_round_add[dec], &warnings);
=======
bool my_time_round(MYSQL_TIME *ltime, uint dec)
{
  int warnings= 0;
  assert(dec <= DATETIME_MAX_DECIMALS);
  /* Add half away from zero */
  bool rc= time_add_nanoseconds_with_round(ltime,
                                           msec_round_add[dec], &warnings);
>>>>>>> upstream/cluster-7.6
  /* Truncate non-significant digits */
  my_time_trunc(ltime, dec);
  return rc;
>>>>>>> pr/231
}

/**
   Propagates a DATETIME_OVERFLOW warning from warnings bitfield to DA in thd.

<<<<<<< HEAD
   @param thd thread context
   @param[in,out] warnings bitfield of warnings set
 */
void propagate_datetime_overflow_helper(THD *thd, int *warnings) {
  if (warnings && (*warnings & MYSQL_TIME_WARN_DATETIME_OVERFLOW) != 0) {
    push_warning_printf(thd, Sql_condition::SL_WARNING,
                        ER_DATETIME_FUNCTION_OVERFLOW,
                        ER_THD(thd, ER_DATETIME_FUNCTION_OVERFLOW), "datetime");
    *warnings &= ~(MYSQL_TIME_WARN_DATETIME_OVERFLOW);
=======
  @param [in,out]  ltime    The value to round.
  @param           dec      Precision.
  @param           truncate Decides whether fractional part of seconds will be
                            truncated/rounded.
  @return                   False on success, true on error.
*/
<<<<<<< HEAD
bool my_time_adjust_frac(MYSQL_TIME *ltime, uint dec, bool truncate) {
  int warnings = 0;
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
=======
bool my_datetime_round(MYSQL_TIME *ltime, uint dec, int *warnings)
{
  assert(dec <= DATETIME_MAX_DECIMALS);
>>>>>>> upstream/cluster-7.6
  /* Add half away from zero */
  bool rc = time_add_nanoseconds_adjust_frac(ltime, msec_round_add[dec],
                                             &warnings, truncate);

  /* Truncate non-significant digits */
  my_time_trunc(ltime, dec);
  return rc;
}

/**
  Round/Truncate datetime value to the given precision.

  @param [in,out]  ltime    The value to round.
  @param           dec      Precision.
  @param [in,out]  warnings Warning flag vector
  @param           truncate Decides whether fractional part of seconds will be
                            truncated/rounded.
  @return                   False on success, true on error.
*/
#ifdef MYSQL_SERVER
bool my_datetime_adjust_frac(MYSQL_TIME *ltime, uint dec, int *warnings,
                             bool truncate) {
  DBUG_ASSERT(dec <= DATETIME_MAX_DECIMALS);
  /* Add half away from zero */
  bool rc = datetime_add_nanoseconds_adjust_frac(ltime, msec_round_add[dec],
                                                 warnings, truncate);
  /* Truncate non-significant digits */
  my_time_trunc(ltime, dec);
  return rc;
}
#endif  // ifdef MYSQL_SERVER

/**
  Round timeval value to the given precision.

  @param [in,out]  tv       The value to round.
  @param           decimals Precision.
  @return                   False on success, true on error.
*/
<<<<<<< HEAD
bool my_timeval_round(struct timeval *tv, uint decimals) {
  DBUG_ASSERT(decimals <= DATETIME_MAX_DECIMALS);
  uint nanoseconds = msec_round_add[decimals];
  tv->tv_usec += (nanoseconds + 500) / 1000;
  if (tv->tv_usec < 1000000) goto ret;
=======
bool my_timeval_round(struct timeval *tv, uint decimals)
{
  assert(decimals <= DATETIME_MAX_DECIMALS);
  uint nanoseconds= msec_round_add[decimals];
  tv->tv_usec+= (nanoseconds + 500) / 1000;
  if (tv->tv_usec < 1000000)
    goto ret;
>>>>>>> upstream/cluster-7.6

  tv->tv_usec = 0;
  tv->tv_sec++;
  if (!IS_TIME_T_VALID_FOR_TIMESTAMP(tv->tv_sec)) {
    tv->tv_sec = TIMESTAMP_MAX_VALUE;
    return true;
  }

ret:
  my_timeval_trunc(tv, decimals);
  return false;
}

/**
  Mix a date value and a time value.

  @param [in,out] ldate  Date value.
  @param          ltime  Time value.
*/
<<<<<<< HEAD
void mix_date_and_time(MYSQL_TIME *ldate, const MYSQL_TIME *ltime) {
  DBUG_ASSERT(ldate->time_type == MYSQL_TIMESTAMP_DATE ||
              ldate->time_type == MYSQL_TIMESTAMP_DATETIME);
=======
void mix_date_and_time(MYSQL_TIME *ldate, const MYSQL_TIME *ltime)
{
  assert(ldate->time_type == MYSQL_TIMESTAMP_DATE ||
         ldate->time_type == MYSQL_TIMESTAMP_DATETIME);
>>>>>>> upstream/cluster-7.6

  if (!ltime->neg && ltime->hour < 24) {
    /*
      Simple case: TIME is within normal 24 hours internal.
      Mix DATE part of ltime2 and TIME part of ltime together.
    */
    ldate->hour = ltime->hour;
    ldate->minute = ltime->minute;
    ldate->second = ltime->second;
    ldate->second_part = ltime->second_part;
  } else {
    /* Complex case: TIME is negative or outside of 24 hours internal. */
    longlong seconds;
    long days, useconds;
<<<<<<< HEAD
    int sign = ltime->neg ? 1 : -1;
    ldate->neg = calc_time_diff(ldate, ltime, sign, &seconds, &useconds);
    DBUG_ASSERT(!ldate->neg);
=======
    int sign= ltime->neg ? 1 : -1;
    ldate->neg= calc_time_diff(ldate, ltime, sign, &seconds, &useconds);
    assert(!ldate->neg);
>>>>>>> upstream/cluster-7.6

    /*
      We pass current date to mix_date_and_time. If we want to use
      this function with arbitrary dates, this code will need
      to cover cases when ltime is negative and "ldate < -ltime".
    */
    assert(ldate->year > 0);

    days = (long)(seconds / SECONDS_IN_24H);
    calc_time_from_sec(ldate, seconds % SECONDS_IN_24H, useconds);
    get_date_from_daynr(days, &ldate->year, &ldate->month, &ldate->day);
  }
  ldate->time_type = MYSQL_TIMESTAMP_DATETIME;
}

/**
  Convert MYSQL_TIME value to its packed numeric representation,
  using field type.
  @param ltime  The value to convert.
  @param type   MySQL field type.
  @retval       Packed numeric representation.
*/
longlong TIME_to_longlong_packed(const MYSQL_TIME *ltime,
                                 enum enum_field_types type) {
  switch (type) {
    case MYSQL_TYPE_TIME:
      return TIME_to_longlong_time_packed(ltime);
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      return TIME_to_longlong_datetime_packed(ltime);
    case MYSQL_TYPE_DATE:
      return TIME_to_longlong_date_packed(ltime);
    default:
      return TIME_to_longlong_packed(ltime);
  }
}

/**
  Convert packed numeric temporal representation to time, date or datetime,
  using field type.
  @param[out] ltime        The variable to write to.
  @param      type         MySQL field type.
  @param      packed_value Numeric datetype representation.
*/
<<<<<<< HEAD
void TIME_from_longlong_packed(MYSQL_TIME *ltime, enum enum_field_types type,
                               longlong packed_value) {
  switch (type) {
    case MYSQL_TYPE_TIME:
      TIME_from_longlong_time_packed(ltime, packed_value);
      break;
    case MYSQL_TYPE_DATE:
      TIME_from_longlong_date_packed(ltime, packed_value);
      break;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      TIME_from_longlong_datetime_packed(ltime, packed_value);
      break;
    default:
      DBUG_ASSERT(0);
      set_zero_time(ltime, MYSQL_TIMESTAMP_ERROR);
      break;
=======
void TIME_from_longlong_packed(MYSQL_TIME *ltime,
                               enum enum_field_types type,
                               longlong packed_value)
{
  switch (type)
  {
  case MYSQL_TYPE_TIME:
    TIME_from_longlong_time_packed(ltime, packed_value);
    break;
  case MYSQL_TYPE_DATE:
    TIME_from_longlong_date_packed(ltime, packed_value);
    break;
  case MYSQL_TYPE_DATETIME:
  case MYSQL_TYPE_TIMESTAMP:
    TIME_from_longlong_datetime_packed(ltime, packed_value);
    break;
  default:
    assert(0);
    set_zero_time(ltime, MYSQL_TIMESTAMP_ERROR);
    break;
>>>>>>> upstream/cluster-7.6
>>>>>>> pr/231
  }
}

/**
  Unpack packed numeric temporal value to date/time value
  and then convert to decimal representation.

  @param [out] dec          The variable to write to.
  @param      type         MySQL field type.
  @param      packed_value Packed numeric temporal representation.
  @return     A decimal value in on of the following formats, depending
              on type: YYYYMMDD, hhmmss.ffffff or YYMMDDhhmmss.ffffff.
*/
my_decimal *my_decimal_from_datetime_packed(my_decimal *dec,
                                            enum enum_field_types type,
                                            longlong packed_value) {
  MYSQL_TIME ltime;
  switch (type) {
    case MYSQL_TYPE_TIME:
      TIME_from_longlong_time_packed(&ltime, packed_value);
      return time2my_decimal(&ltime, dec);
    case MYSQL_TYPE_DATE:
      TIME_from_longlong_date_packed(&ltime, packed_value);
      ulonglong2decimal(TIME_to_ulonglong_date(ltime), dec);
      return dec;
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      TIME_from_longlong_datetime_packed(&ltime, packed_value);
      return date2my_decimal(&ltime, dec);
    default:
      assert(0);
      ulonglong2decimal(0, dec);
      return dec;
  }
}

/**
   This function gets GMT time and adds value of time_zone to get
   the local time. This function is used when server wants a timestamp
   value from dictionary system.

   @param gmt_time     GMT time value.
   @return time as ulonglong
*/
<<<<<<< HEAD
=======
longlong longlong_from_datetime_packed(enum enum_field_types type,
                                       longlong packed_value) {
  MYSQL_TIME ltime;
  switch (type) {
    case MYSQL_TYPE_TIME:
      TIME_from_longlong_time_packed(&ltime, packed_value);
      return TIME_to_ulonglong_time(&ltime);
    case MYSQL_TYPE_DATE:
      TIME_from_longlong_date_packed(&ltime, packed_value);
      return TIME_to_ulonglong_date(&ltime);
    case MYSQL_TYPE_DATETIME:
    case MYSQL_TYPE_TIMESTAMP:
      TIME_from_longlong_datetime_packed(&ltime, packed_value);
      return TIME_to_ulonglong_datetime(&ltime);
    default:
      assert(0);
      return 0;
  }
}

/**
  Convert packed numeric temporal representation to unpacked numeric
  representation.
  @param type           MySQL field type.
  @param packed_value   Numeric packed temporal representation.
  @return               A double value in on of the following formats,
                        depending  on type:
                        YYYYMMDD, hhmmss.ffffff or YYMMDDhhmmss.ffffff.
*/
double double_from_datetime_packed(enum enum_field_types type,
                                   longlong packed_value) {
  longlong result = longlong_from_datetime_packed(type, packed_value);
  return result +
         ((double)MY_PACKED_TIME_GET_FRAC_PART(packed_value)) / 1000000;
}

  /**
    This function gets GMT time and adds value of time_zone to get
    the local time. This function is used when server wants a timestamp
    value from dictionary system.

    @param  gmt_time     GMT time value.
  */

#ifdef MYSQL_SERVER
>>>>>>> pr/231
ulonglong gmt_time_to_local_time(ulonglong gmt_time) {
  MYSQL_TIME time;
  bool not_used;

  THD *thd = current_thd;
  Time_zone *tz = thd->variables.time_zone;

  // Convert longlong time to MYSQL_TIME format
  my_longlong_to_datetime_with_warn(gmt_time, &time, MYF(0));

  // Convert MYSQL_TIME to epoc second according to GMT time_zone.
  my_time_t timestamp;
  timestamp = my_tz_OFFSET0->TIME_to_gmt_sec(&time, &not_used);

  // Convert epoc seconds to local time
  tz->gmt_sec_to_TIME(&time, timestamp);

  // Return ulonglong value from MYSQL_TIME
  return TIME_to_ulonglong_datetime(time);
}

MYSQL_TIME my_time_set(uint y, uint m, uint d, uint h, uint mi, uint s,
                       unsigned long ms, bool negative,
                       enum_mysql_timestamp_type type) {
  return {y, m, d, h, mi, s, ms, negative, type, 0};
}

uint actual_decimals(const MYSQL_TIME *ts) {
  uint count = DATETIME_MAX_DECIMALS;
  for (int i = 1; i <= DATETIME_MAX_DECIMALS; i++) {
    if (ts->second_part % log_10_int[i] != 0) break;
    count--;
  }
  return count;
}

size_t max_fraction(uint decimals) {
  size_t res = 0;
  for (uint i = 1; i <= DATETIME_MAX_DECIMALS; i++) {
    res *= 10;
    if (i <= decimals) res += 9;
  }
  return res;
}

/**
   @} (end of defgroup SQL_TIME)
*/
