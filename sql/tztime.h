#ifndef TZTIME_INCLUDED
#define TZTIME_INCLUDED

#include "my_config.h"
/* Copyright (c) 2004, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#include "my_inttypes.h"

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef _WIN32
#include <winsock2.h>
#endif

#include "my_time_t.h"   // my_time_t
#include "mysql_time.h"  // MYSQL_TIME

class String;
class THD;

/**
  This class represents abstract time zone and provides
  basic interface for MYSQL_TIME <-> my_time_t conversion.
  Actual time zones which are specified by DB, or via offset
  or use system functions are its descendants.
*/
class Time_zone {
 public:
  /**
    Enum to identify the type of the timezone
  */
  enum tz_type { TZ_DB = 1, TZ_OFFSET = 2, TZ_SYSTEM = 3, TZ_UTC = 4 };

  /**
    Converts local time in MYSQL_TIME representation to
    my_time_t (UTC seconds since Epoch) representation.
    Returns 0 in case of error. Sets in_dst_time_gap to true if date provided
    falls into spring time-gap (or lefts it untouched otherwise).
  */
  virtual my_time_t TIME_to_gmt_sec(const MYSQL_TIME *t,
                                    bool *in_dst_time_gap) const = 0;
  /**
   Converts UTC epoch seconds to time in MYSQL_TIME representation.

   @param[out]   tmp  equivalent time point in MYSQL_TIME representation
   @param[in]    t    number of seconds in UNIX epoch
   */
  virtual void gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const = 0;
  /**
    Converts UTC epoch seconds and microseconds to time in
    MYSQL_TIME representation.

    @param[in]    tv   number of seconds and microseconds in UNIX epoch
    @param[out]   tmp  equivalent time point in MYSQL_TIME representation
  */
  void gmt_sec_to_TIME(MYSQL_TIME *tmp, my_timeval tv) const {
    gmt_sec_to_TIME(tmp, (my_time_t)tv.m_tv_sec);
    tmp->second_part = tv.m_tv_usec;
  }
  /**
    Because of constness of String returned by get_name() time zone name
    have to be already zeroended to be able to use String::ptr() instead
    of c_ptr().
  */
  virtual const String *get_name() const = 0;
  /**
   *Returns the timezone type set.
   */
  virtual tz_type get_timezone_type() const = 0;
  /**
    Returns the offset set for a Timezone offset.
    This function has to be invoked ONLY when TZ_OFFSET is set.
  */
  virtual long get_timezone_offset() const = 0;
  /**
    We need this only for suppressing warnings, objects of this type are
    allocated on MEM_ROOT and should not require destruction.
  */
  virtual ~Time_zone() = default;

 protected:
  static inline void adjust_leap_second(MYSQL_TIME *t);
};

extern Time_zone *my_tz_UTC;
extern Time_zone *my_tz_SYSTEM;
extern Time_zone *my_tz_OFFSET0;
extern Time_zone *my_tz_find(THD *thd, const String *name);
extern bool my_tz_init(THD *org_thd, const char *default_tzname,
                       bool bootstrap);
extern void my_tz_free();
extern my_time_t sec_since_epoch_TIME(MYSQL_TIME *t);

bool check_time_zone_convertibility(const MYSQL_TIME &mt);
bool convert_time_zone_displacement(const Time_zone *tz, MYSQL_TIME *mt);
my_time_t use_input_time_zone(const MYSQL_TIME *input, bool *in_dst_time_gap);
void sec_to_TIME(MYSQL_TIME *tmp, my_time_t t, int64 offset);

/**
  Number of elements in table list produced by my_tz_get_table_list()
  (this table list contains tables which are needed for dynamical loading
  of time zone descriptions). Actually it is implementation detail that
  should not be used anywhere outside of tztime.h and tztime.cc.
*/

static const int MY_TZ_TABLES_COUNT = 4;

#endif /* TZTIME_INCLUDED */
