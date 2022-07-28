/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#include "sql/time_zone_common.h"

#include <algorithm>

#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>

#include "lex_string.h"
#include "m_ctype.h"
#include "m_string.h"  // strmake
#include "map_helpers.h"
#include "my_alloc.h"
#include "my_base.h"
#include "my_compiler.h"
#include "my_dbug.h"
#include "my_dir.h"
#include "my_inttypes.h"
#include "my_io.h"
#include "my_loglevel.h"
#include "my_macros.h"
#include "my_pointer_arithmetic.h"
#include "my_psi_config.h"
#include "my_sys.h"
#include "my_time.h"  // MY_TIME_T_MIN
#include "mysql/components/services/bits/mysql_mutex_bits.h"
#include "mysql/components/services/bits/psi_bits.h"
#include "mysql/components/services/bits/psi_memory_bits.h"
#include "mysql/components/services/bits/psi_mutex_bits.h"
#include "mysql/components/services/log_builtins.h"
#include "mysql/components/services/log_shared.h"
#include "mysql/psi/mysql_file.h"
#include "mysql/psi/mysql_memory.h"
#include "mysql/psi/mysql_mutex.h"
#include "mysqld_error.h"
#include "sql/dd/types/event.h"
#include "sql/field.h"
#include "sql/handler.h"
#include "sql/psi_memory_key.h"
#include "sql/sql_const.h"
#include "sql/sql_error.h"
#include "sql/system_variables.h"
#include "sql/thr_malloc.h"
#include "sql/tzfile.h"  // TZ_MAX_REV_RANGES
#include "template_utils.h"
#include "thr_lock.h"
#include "thr_mutex.h"

#include <algorithm>
#include <string>
#include <unordered_map>
#include <utility>

#include "print_version.h"
#include "welcome_copyright_notice.h" /* ORACLE_WELCOME_COPYRIGHT_NOTICE */

using std::min;

/*
   Most of the following code and structures were derived from
   public domain code from ftp://elsie.nci.nih.gov/pub
   (We will refer to this code as to elsie-code further.)
*/

bool prepare_tz_info(TIME_ZONE_INFO *sp, MEM_ROOT *storage) {
  // We must allow values smaller than MYTIME_MIN_VALUE here (negative values)
  // and values larger than MYTIME_MAX_VALUE
  constexpr my_time_t MYTIME_MIN = std::numeric_limits<my_time_t>::min();
  constexpr my_time_t MYTIME_MAX = std::numeric_limits<my_time_t>::max();
  my_time_t cur_t = MYTIME_MIN;
  my_time_t cur_l, end_t, end_l = 0;
  my_time_t cur_max_seen_l = MYTIME_MIN;
  long cur_offset, cur_corr, cur_off_and_corr;
  uint next_trans_idx, next_leap_idx;
  uint i;
  /*
    Temporary arrays where we will store tables. Needed because
    we don't know table sizes ahead. (Well we can estimate their
    upper bound but this will take extra space.)
  */
  my_time_t revts[TZ_MAX_REV_RANGES];
  REVT_INFO revtis[TZ_MAX_REV_RANGES];

  /*
    Let us setup fallback time type which will be used if we have not any
    transitions or if we have moment of time before first transition.
    We will find first non-DST local time type and use it (or use first
    local time type if all of them are DST types).
  */
  for (i = 0; i < sp->typecnt && sp->ttis[i].tt_isdst; i++) /* no-op */
    ;
  if (i == sp->typecnt) i = 0;
  sp->fallback_tti = &(sp->ttis[i]);

  /*
    Let us build shifted my_time_t -> my_time_t map.
  */
  sp->revcnt = 0;

  /* Let us find initial offset */
  if (sp->timecnt == 0 || cur_t < sp->ats[0]) {
    /*
      If we have not any transitions or t is before first transition we are
      using already found fallback time type which index is already in i.
    */
    next_trans_idx = 0;
  } else {
    /* cur_t == sp->ats[0] so we found transition */
    i = sp->types[0];
    next_trans_idx = 1;
  }

  cur_offset = sp->ttis[i].tt_gmtoff;

  /* let us find leap correction... unprobable, but... */
  for (next_leap_idx = 0;
       next_leap_idx < sp->leapcnt && cur_t >= sp->lsis[next_leap_idx].ls_trans;
       ++next_leap_idx)
    continue;

  if (next_leap_idx > 0)
    cur_corr = sp->lsis[next_leap_idx - 1].ls_corr;
  else
    cur_corr = 0;

  /* Iterate through t space */
  while (sp->revcnt < TZ_MAX_REV_RANGES - 1) {
    cur_off_and_corr = cur_offset - cur_corr;

    /*
      We assuming that cur_t could be only overflowed downwards,
      we also assume that end_t won't be overflowed in this case.
    */
    if (cur_off_and_corr < 0 && cur_t < MYTIME_MIN - cur_off_and_corr)
      cur_t = MYTIME_MIN - cur_off_and_corr;

    cur_l = cur_t + cur_off_and_corr;

    /*
      Let us choose end_t as point before next time type change or leap
      second correction.
    */
    end_t =
        min((next_trans_idx < sp->timecnt) ? sp->ats[next_trans_idx] - 1
                                           : MYTIME_MAX,
            (next_leap_idx < sp->leapcnt) ? sp->lsis[next_leap_idx].ls_trans - 1
                                          : MYTIME_MAX);
    /*
      again assuming that end_t can be overlowed only in positive side
      we also assume that end_t won't be overflowed in this case.
    */
    if (cur_off_and_corr > 0 && end_t > MYTIME_MAX - cur_off_and_corr)
      end_t = MYTIME_MAX - cur_off_and_corr;

    end_l = end_t + cur_off_and_corr;

    if (end_l > cur_max_seen_l) {
      /* We want special handling in the case of first range */
      if (cur_max_seen_l == MYTIME_MIN) {
        revts[sp->revcnt] = cur_l;
        revtis[sp->revcnt].rt_offset = cur_off_and_corr;
        revtis[sp->revcnt].rt_type = 0;
        sp->revcnt++;
        cur_max_seen_l = end_l;
      } else {
        if (cur_l > cur_max_seen_l + 1) {
          /* We have a spring time-gap and we are not at the first range */
          revts[sp->revcnt] = cur_max_seen_l + 1;
          revtis[sp->revcnt].rt_offset = revtis[sp->revcnt - 1].rt_offset;
          revtis[sp->revcnt].rt_type = 1;
          sp->revcnt++;
          if (sp->revcnt == TZ_MAX_TIMES + TZ_MAX_LEAPS + 1)
            break; /* That was too much */
          cur_max_seen_l = cur_l - 1;
        }

        /* Assume here end_l > cur_max_seen_l (because end_l>=cur_l) */

        revts[sp->revcnt] = cur_max_seen_l + 1;
        revtis[sp->revcnt].rt_offset = cur_off_and_corr;
        revtis[sp->revcnt].rt_type = 0;
        sp->revcnt++;
        cur_max_seen_l = end_l;
      }
    }

    if (end_t == MYTIME_MAX ||
        ((cur_off_and_corr > 0) && (end_t >= MYTIME_MAX - cur_off_and_corr)))
      /* end of t space */
      break;

    cur_t = end_t + 1;

    /*
      Let us find new offset and correction. Because of our choice of end_t
      cur_t can only be point where new time type starts or/and leap
      correction is performed.
    */
    if (sp->timecnt != 0 && cur_t >= sp->ats[0]) /* else reuse old offset */
      if (next_trans_idx < sp->timecnt && cur_t == sp->ats[next_trans_idx]) {
        /* We are at offset point */
        cur_offset = sp->ttis[sp->types[next_trans_idx]].tt_gmtoff;
        ++next_trans_idx;
      }

    if (next_leap_idx < sp->leapcnt &&
        cur_t == sp->lsis[next_leap_idx].ls_trans) {
      /* we are at leap point */
      cur_corr = sp->lsis[next_leap_idx].ls_corr;
      ++next_leap_idx;
    }
  }

  /* check if we have had enough space */
  if (sp->revcnt == TZ_MAX_REV_RANGES - 1) return true;

  /* set maximum end_l as finisher */
  revts[sp->revcnt] = end_l;

  /* Allocate arrays of proper size in sp and copy result there */
  if (!(sp->revts = (my_time_t *)storage->Alloc(sizeof(my_time_t) *
                                                (sp->revcnt + 1))) ||
      !(sp->revtis =
            (REVT_INFO *)storage->Alloc(sizeof(REVT_INFO) * sp->revcnt)))
    return true;

  memcpy(sp->revts, revts, sizeof(my_time_t) * (sp->revcnt + 1));
  memcpy(sp->revtis, revtis, sizeof(REVT_INFO) * sp->revcnt);

  return false;
}

/*
  End of elsie derived code.
*/
