#ifndef TZTIME_INCLUDED
#define TZTIME_INCLUDED

/* Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

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


#ifdef USE_PRAGMA_INTERFACE
#pragma interface			/* gcc class interface */
#endif

#include "my_time.h"                            /* my_time_t */
#include "mysql_time.h"                         /* MYSQL_TIME */
#include "sql_list.h"                           /* Sql_alloc */
#include "sql_string.h"                         /* String */

class THD;

#if !defined(TESTTIME) && !defined(TZINFO2SQL)

class THD;

/**
  This class represents abstract time zone and provides 
  basic interface for MYSQL_TIME <-> my_time_t conversion.
  Actual time zones which are specified by DB, or via offset 
  or use system functions are its descendants.
*/
class Time_zone: public Sql_alloc 
{
public:
  Time_zone() {}                              /* Remove gcc warning */
  /**
    Converts local time in broken down MYSQL_TIME representation to 
    my_time_t (UTC seconds since Epoch) represenation.
    Returns 0 in case of error. May set error_code to ER_WARN_DATA_OUT_OF_RANGE
    or ER_WARN_INVALID_TIMESTAMP, see TIME_to_timestamp())
  */
  virtual my_time_t TIME_to_gmt_sec(const MYSQL_TIME *t, 
                                    uint *error_code) const = 0;
  /**
    Converts time in my_time_t representation to local time in
    broken down MYSQL_TIME representation.
  */
  virtual void   gmt_sec_to_TIME(MYSQL_TIME *tmp, my_time_t t) const = 0;
  /**
    Because of constness of String returned by get_name() time zone name 
    have to be already zeroended to be able to use String::ptr() instead
    of c_ptr().
  */
  virtual const String * get_name() const = 0;

  /** 
    We need this only for surpressing warnings, objects of this type are
    allocated on MEM_ROOT and should not require destruction.
  */
  virtual ~Time_zone() {};

protected:
  static inline void adjust_leap_second(MYSQL_TIME *t);
};

extern Time_zone * my_tz_UTC;
extern Time_zone * my_tz_SYSTEM;
extern Time_zone * my_tz_OFFSET0;
extern Time_zone * my_tz_find(THD *thd, const String *name);
extern my_bool     my_tz_init(THD *org_thd, const char *default_tzname, my_bool bootstrap);
extern void        my_tz_free();
extern my_time_t   sec_since_epoch_TIME(MYSQL_TIME *t);

/**
  Number of elements in table list produced by my_tz_get_table_list()
  (this table list contains tables which are needed for dynamical loading
  of time zone descriptions). Actually it is imlementation detail that
  should not be used anywhere outside of tztime.h and tztime.cc.
*/

static const int MY_TZ_TABLES_COUNT= 4;


#endif /* !defined(TESTTIME) && !defined(TZINFO2SQL) */
#endif /* TZTIME_INCLUDED */
