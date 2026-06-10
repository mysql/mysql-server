/*
   Copyright (c) 2025, 2026, Oracle and/or its affiliates.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>

#include "ndb_config.h"
#include "portlib/NdbTimestamp.h"
#include "util/require.h"
#include "util/span.h"

#ifdef _WIN32
static inline struct tm *gmtime_r(const time_t *pt, struct tm *ptm) {
  return gmtime_s(ptm, pt) == 0 ? ptm : nullptr;
}

static inline struct tm *localtime_r(const time_t *pt, struct tm *ptm) {
  return localtime_s(ptm, pt) == 0 ? ptm : nullptr;
}
#endif

#if !defined(NDEBUG) || !defined(HAVE_TM_GMTOFF)

/*
 * On platforms where struct tm does not have tm_gmtoff field (Windows and
 * Solaris) the below code will calculate tm_gmtoff if needed and caches it.
 *
 * The cache can contain offset for standard time and dayligt saving time for
 * one odd and one even year, this to avoid potentially repeatable invalidation
 * and recalculation around new year where timestamps from old year may mix with
 * timestamps from new year.
 *
 * Normally these functions are only used for current time and needing cache for
 * two years should be rare. Also timezones normally use same GMT offset from
 * year to year, but some day some timezone may announce that they will change
 * for example their use of daylight saving time.
 *
 * The calculation of GMT offset is done by comparing the time part of time
 * (HH:MM:SS) component from system timezone and GMT. It assumes that the date
 * part may be off by at most one day. That is true as long as GMT offset is
 * less than the shortest day. Today all known timezones that uses daylight
 * saving time adjusts time with one hour and shortest day may be 23 hours. And
 * timezones are between -11:00 (Pacific/Pago_Pago) and +14:00
 * (Pacific/Kiritimati).
 */

static int calculate_gmtoff(const time_t *pt, const struct tm *ptm) {
  struct tm gtm;
  gmtime_r(pt, &gtm);
  if (ptm->tm_year == gtm.tm_year && ptm->tm_yday == gtm.tm_yday) {
    // no adjustment needed
  } else if (ptm->tm_year == gtm.tm_year) {
    int yday_diff = ptm->tm_yday - gtm.tm_yday;
    // Only adjusting hours since date will not be used below
    gtm.tm_hour -= yday_diff * 24;
  } else {
    // Assume only one year difference, and one is new years eve and the other
    // day after.
    int yday_diff = ptm->tm_year - gtm.tm_year;
    // Only adjusting hours since date will not be used below
    gtm.tm_hour -= yday_diff * 24;
#if !defined(NDEBUG)
    /* Check that assumptions are correct:
     * - year differ by one
     * - the newer year has yday=1 (Jan 1)
     * - the older year is on its last day (Dec 31)
     */
    if (gtm.tm_year < ptm->tm_year) {
      require(gtm.tm_year + 1 == ptm->tm_year);
      require(ptm->tm_yday == 0);
      require(gtm.tm_mon == 11);
      require(gtm.tm_mday == 31);
    } else {
      require(ptm->tm_year + 1 == gtm.tm_year);
      require(gtm.tm_yday == 0);
      require(ptm->tm_mon == 11);
      require(ptm->tm_mday == 31);
    }
#endif
  }
  int gmtoff = (ptm->tm_sec - gtm.tm_sec) + 60 * (ptm->tm_min - gtm.tm_min) +
               3600 * (ptm->tm_hour - gtm.tm_hour);
#ifdef HAVE_TM_GMTOFF
  require(gmtoff == ptm->tm_gmtoff);
#endif
  return gmtoff;
}

#ifndef HAVE_TM_GMTOFF
struct year_gmtoff {
  int32_t year;
  int32_t gmtoff;
};
static_assert(std::atomic<year_gmtoff>::is_always_lock_free);
static constexpr int32_t bad_gmtoff = UINT32_MAX;

static std::atomic<year_gmtoff> year_gmtoff_arr[2 /* year&1 */][2 /* isdst */];

static void init_cached_gmtoff() {
  for (auto &year_entry : year_gmtoff_arr)
    for (auto &dst_entry : year_entry)
      dst_entry.store({0, 0}, std::memory_order_relaxed);
}

static int get_cached_gmtoff(int year, int isdst) {
  year_gmtoff v =
      year_gmtoff_arr[year & 1][isdst].load(std::memory_order_relaxed);
  if (v.year != year) return bad_gmtoff;
  return v.gmtoff;
}

static void set_cached_gmtoff(int year, int isdst, int gmtoff) {
  year_gmtoff v = {year, gmtoff};
  year_gmtoff_arr[year & 1][isdst].store(v, std::memory_order_relaxed);
}
#endif

static int get_gmtoff(const time_t *pt [[maybe_unused]], const struct tm *ptm) {
#ifdef HAVE_TM_GMTOFF
  assert(ptm->tm_gmtoff == calculate_gmtoff(pt, ptm));
  return ptm->tm_gmtoff;
#else
  int gmtoff = get_cached_gmtoff(ptm->tm_year, ptm->tm_isdst);
  if (gmtoff != bad_gmtoff) {
    assert(gmtoff == calculate_gmtoff(pt, ptm));
    return gmtoff;
  }
  gmtoff = calculate_gmtoff(pt, ptm);
  set_cached_gmtoff(ptm->tm_year, ptm->tm_isdst, gmtoff);
  return gmtoff;
#endif
}

#endif

void NdbTimestamp_Reset() {
  tzset();
#ifndef HAVE_TM_GMTOFF
  init_cached_gmtoff();
#endif
}

std::timespec NdbTimestamp_GetCurrentTime() {
  std::timespec ts;
  require(std::timespec_get(&ts, TIME_UTC) == TIME_UTC);
  return ts;
}

int NdbTimestamp_GetUtcComponents(const std::timespec *t,
                                  NdbTimestampComponents *tm) {
  time_t sec = t->tv_sec;
  struct tm tmbuf;
  if (gmtime_r(&sec, &tmbuf) == nullptr) return -1;
  tm->year = tmbuf.tm_year + 1900;
  tm->mon = tmbuf.tm_mon + 1;
  tm->mday = tmbuf.tm_mday;
  tm->hour = tmbuf.tm_hour;
  tm->min = tmbuf.tm_min;
  tm->sec = tmbuf.tm_sec;
  tm->nsec = t->tv_nsec;
  tm->gmtoff = 0;
  return 0;
}

int NdbTimestamp_GetLocalComponents(const std::timespec *t,
                                    NdbTimestampComponents *tm) {
  time_t sec = t->tv_sec;
  struct tm tmbuf;
  if (localtime_r(&sec, &tmbuf) == nullptr) return -1;
  tm->year = tmbuf.tm_year + 1900;
  tm->mon = tmbuf.tm_mon + 1;
  tm->mday = tmbuf.tm_mday;
  tm->hour = tmbuf.tm_hour;
  tm->min = tmbuf.tm_min;
  tm->sec = tmbuf.tm_sec;
  tm->nsec = t->tv_nsec;
#ifdef HAVE_TM_GMTOFF
  tm->gmtoff = tmbuf.tm_gmtoff;
  assert(tm->gmtoff == get_gmtoff(&sec, &tmbuf));
#else
  tm->gmtoff = get_gmtoff(&sec, &tmbuf);
#endif
  return 0;
}

static NdbTimestampStringFormat default_format =
    NdbTimestampStringFormat::Iso8601Utc;

/*
 * unsigned_integer_to_zero_padded_string
 *
 * Writes decimal representation of unsigned integer, x, into given buffer, zero
 * padded prefix to full buffer.
 *
 * Note, given integer value does not fit in buffer function abort program.
 *
 * Note, no null termination. Returns buffer end (next position after last
 * digit).
 *
 */
static char *unsigned_integer_to_zero_padded_string(ndb::span<char> buf,
                                                    unsigned x) {
  size_t i = buf.size();
  while (x > 0 && i > 0) {
    i--;
    buf[i] = '0' + (x % 10);
    x /= 10;
  }
  require(x == 0);
  while (i > 0) {
    i--;
    buf[i] = '0';
  }
  return buf.end();
}

static int NdbTimestamp_FormatString(ndb::span<char> buf,
                                     NdbTimestampStringFormat format,
                                     const NdbTimestampComponents *tm) {
  if (format == NdbTimestampStringFormat::DefaultFormat)
    format = default_format;
  int timesep;
  int usec;
  int timezone;
  size_t length;
  switch (format) {
    case NdbTimestampStringFormat::LegacyFormat:
      timesep = 1;
      usec = 0;
      timezone = 0;
      length = 19;  // 10 + 1 + 8;
      break;
    case NdbTimestampStringFormat::Iso8601Utc:
      require(tm->gmtoff == 0);
      timesep = 2;
      usec = 1;
      timezone = 1;
      length = 27;  // 10 + 1 + 8 + 7 + 1;
      break;
    case NdbTimestampStringFormat::Iso8601SystemTime:
      timesep = 2;
      usec = 1;
      timezone = 2;
      length = 32;  // 10 + 1 + 8 + 7 + 6;
      break;
    default:
      // unreachable
      abort();
  }
  if (length > buf.size()) {
    // Too small buffer
    return -1;
  }
  char *p = buf.data();
  // date
  {
    p = unsigned_integer_to_zero_padded_string({p, 4}, tm->year);
    *p++ = '-';
    p = unsigned_integer_to_zero_padded_string({p, 2}, tm->mon);
    *p++ = '-';
    p = unsigned_integer_to_zero_padded_string({p, 2}, tm->mday);
  }
  // time separator
  switch (timesep) {
    case 0:
      break;
    case 1:
      *p++ = ' ';
      break;
    case 2:
      *p++ = 'T';
      break;
  }
  // time
  {
    p = unsigned_integer_to_zero_padded_string({p, 2}, tm->hour);
    *p++ = ':';
    p = unsigned_integer_to_zero_padded_string({p, 2}, tm->min);
    *p++ = ':';
    p = unsigned_integer_to_zero_padded_string({p, 2}, tm->sec);
  }
  // usec
  if (usec == 1) {
    *p++ = '.';
    p = unsigned_integer_to_zero_padded_string({p, 6}, tm->nsec / 1000);
  }
  // timezone
  switch (timezone) {
    case 0:
      break;
    case 1:
      *p++ = 'Z';
      break;
    case 2: {
      int gmtoff = tm->gmtoff;
      if (gmtoff < 0) {
        *p++ = '-';
        gmtoff = -gmtoff;
      } else {
        *p++ = '+';
      }
      int h = gmtoff / 3600;
      int m = gmtoff / 60 % 60;
      p = unsigned_integer_to_zero_padded_string({p, 2}, h);
      *p++ = ':';
      p = unsigned_integer_to_zero_padded_string({p, 2}, m);
    } break;
    default:
      // unreachable
      abort();
  }
  *p = 0;
  assert(p == buf.begin() + length);
  return length;
}

int NdbTimestamp_GetAsString(ndb::span<char> buf,
                             NdbTimestampStringFormat format,
                             const std::timespec *pt,
                             const NdbTimestampComponents *ptm) {
  std::timespec t;
  NdbTimestampComponents tm;
  int ok = 0;
  if (ptm != nullptr)
    tm = *ptm;
  else {
    if (pt != nullptr)
      t = *pt;
    else
      t = NdbTimestamp_GetCurrentTime();
    if (format == NdbTimestampStringFormat::DefaultFormat)
      format = default_format;
    switch (format) {
      case NdbTimestampStringFormat::LegacyFormat:
      case NdbTimestampStringFormat::Iso8601SystemTime:
        ok = NdbTimestamp_GetLocalComponents(&t, &tm);
        break;
      case NdbTimestampStringFormat::Iso8601Utc:
        ok = NdbTimestamp_GetUtcComponents(&t, &tm);
        break;
      default:
        abort();
    }
  }
  if (ok == -1) return -1;
  ok = NdbTimestamp_FormatString(buf, format, &tm);
  return ok;
}

int NdbTimestamp_SetDefaultStringFormat(NdbTimestampStringFormat format) {
  switch (format) {
    case NdbTimestampStringFormat::DefaultFormat:
      default_format = NdbTimestampStringFormat::Iso8601Utc;
      return 0;
    case NdbTimestampStringFormat::LegacyFormat:
    case NdbTimestampStringFormat::Iso8601Utc:
    case NdbTimestampStringFormat::Iso8601SystemTime:
      default_format = format;
      return 0;
    default:
      abort();
  }
  return -1;
}

int NdbTimestamp_GetDefaultStringFormatLength() {
  switch (default_format) {
    case NdbTimestampStringFormat::DefaultFormat:
      // default_format should never be DefaultFormat
      assert(false);
      return -1;
    case NdbTimestampStringFormat::LegacyFormat:
      return 19;
    case NdbTimestampStringFormat::Iso8601Utc:
      return 27;
    case NdbTimestampStringFormat::Iso8601SystemTime:
      return 32;
  }
  return -1;
}

#ifdef TEST_NDBTIMESTAMP

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "unittest/mytap/tap.h"

/*
 * On Windows setting timezone with TZ environment variable and tzset funtion
 * will not work as for POSIX-like systems.
 *   - TZ variable only support the "std offset dst" format, not the POSIX
 *     ":[filespec]" format, nor the
 *     "std offset[dst[offset][,start[/time],end[/time]]]" format that can be
 *     used to define the DST rules.
 *   - From MSDN article for _tzset: "The C run-time library assumes the
 *     United States' rules for implementing the calculation of daylight
 *     saving time (DST)."
 * Skipping check of Stockholm timezone on Windows.
 * Also since Windows only supports three letter timezone in TZ UTC-14 will be
 * used instead of LINT-14.
 *
 *   - timezones[][0] is used on POSIX-like systems
 *   - timezones[][1] is used on Windows
 */
static const char *timezones[5][2] = {
    {":Etc/UTC" /* "UTC" */, "UTC"},
    {":Europe/Stockholm" /* "CET-01CEST-02,M3.5.0,M10.5.0" */, nullptr},
    {":America/Los_Angeles" /* "PST+08PDT+07,M3.3.0,M11.1.0" */, "PST+08PDT"},
    {":Pacific/Pago_Pago" /* "SST+11" */, "SST+11"},
    {":Pacific/Kiritimati" /* "LINT-14" */, "UTC-14"}};

static struct {
  time_t t;
  const char *s[5];  // In same order as timezones above
} times[5] = {
    {1735691400,
     {"2025-01-01T00:30:00.012345Z", "2025-01-01T01:30:00.012345+01:00",
      "2024-12-31T16:30:00.012345-08:00", "2024-12-31T13:30:00.012345-11:00",
      "2025-01-01T14:30:00.012345+14:00"}},
    {1747701000,
     {"2025-05-20T00:30:00.012345Z", "2025-05-20T02:30:00.012345+02:00",
      "2025-05-19T17:30:00.012345-07:00", "2025-05-19T13:30:00.012345-11:00",
      "2025-05-20T14:30:00.012345+14:00"}},
    {1750379400,
     {"2025-06-20T00:30:00.012345Z", "2025-06-20T02:30:00.012345+02:00",
      "2025-06-19T17:30:00.012345-07:00", "2025-06-19T13:30:00.012345-11:00",
      "2025-06-20T14:30:00.012345+14:00"}},
    {1761953400,
     {"2025-10-31T23:30:00.012345Z", "2025-11-01T00:30:00.012345+01:00",
      "2025-10-31T16:30:00.012345-07:00", "2025-10-31T12:30:00.012345-11:00",
      "2025-11-01T13:30:00.012345+14:00"}},
    {1767223800,
     {"2025-12-31T23:30:00.012345Z", "2026-01-01T00:30:00.012345+01:00",
      "2025-12-31T15:30:00.012345-08:00", "2025-12-31T12:30:00.012345-11:00",
      "2026-01-01T13:30:00.012345+14:00"}}};

static void test_UTC() {
  std::timespec t = NdbTimestamp_GetCurrentTime();
  time_t ut = t.tv_sec;

  NdbTimestampComponents tc;
  struct tm tm;
  NdbTimestamp_GetUtcComponents(&t, &tc);
  gmtime_r(&ut, &tm);
  ok1(tc.year == tm.tm_year + 1900);
  ok1(tc.mon == tm.tm_mon + 1);
  ok1(tc.mday == tm.tm_mday);
  ok1(tc.hour == tm.tm_hour);
  ok1(tc.min == tm.tm_min);
  ok1(tc.sec == tm.tm_sec);
  ok1(tc.nsec == t.tv_nsec);
#ifdef HAVE_TM_GMTOFF
  ok1(tc.gmtoff == tm.tm_gmtoff);
#else
  skip(1, "No tm.tm_gmtoff on this platform");
#endif

  char buf[2][100];

  // Adjust nano seconds to match expected result
  tc.nsec = 12345678;
  t.tv_nsec = 12345678;

  NdbTimestamp_GetAsString({buf[0], sizeof(buf[0])},
                           NdbTimestampStringFormat::Iso8601Utc, nullptr, &tc);
  strftime(buf[1], sizeof(buf[1]), "%FT%T.012345Z", &tm);
  ok1(strcmp(buf[0], buf[1]) == 0);

  constexpr int iutc = 0;  // First timezone is UTC
  for (size_t i = 0; i < std::size(times); i++) {
    t.tv_sec = times[i].t;
    NdbTimestamp_GetAsString({buf[0], sizeof(buf[0])},
                             NdbTimestampStringFormat::Iso8601Utc, &t);
    ok1(strcmp(buf[0], times[i].s[iutc]) == 0);
  }
}

static void test_TZ(int itz) {
#ifndef _WIN32
  const char *tzenv = timezones[itz][0];
  setenv("TZ", tzenv, 1);
#else
  const char *tzenv = timezones[itz][1];
  if (tzenv == nullptr) {
    skip(16, "Timezone '%s' not supported on Windows.", timezones[itz][0]);
    return;
  }
  _putenv_s("TZ", tzenv);
#endif
  printf("TZ=%s\n", tzenv);
  NdbTimestamp_Reset();

  std::timespec t = NdbTimestamp_GetCurrentTime();
  time_t ut = t.tv_sec;

  NdbTimestampComponents tc;
  struct tm tm;
  NdbTimestamp_GetLocalComponents(&t, &tc);
  localtime_r(&ut, &tm);
  ok1(tc.year == tm.tm_year + 1900);
  ok1(tc.mon == tm.tm_mon + 1);
  ok1(tc.mday == tm.tm_mday);
  ok1(tc.hour == tm.tm_hour);
  ok1(tc.min == tm.tm_min);
  ok1(tc.sec == tm.tm_sec);
  ok1(tc.nsec == t.tv_nsec);
#ifdef HAVE_TM_GMTOFF
  ok1(tc.gmtoff == tm.tm_gmtoff);
#else
  skip(1, "No tm.tm_gmtoff on this platform");
#endif

  char buf[2][100];

  NdbTimestamp_GetAsString({buf[0], sizeof(buf[0])},
                           NdbTimestampStringFormat::LegacyFormat, nullptr,
                           &tc);
  strftime(buf[1], sizeof(buf[1]), "%F %T", &tm);
  ok1(strcmp(buf[0], buf[1]) == 0);

  // Only compare up to GMT hour offset since format of GMT offset may differ
  // (+12:34 vs +1234)
  ok1(strncmp(buf[0], buf[1], 29) == 0);

  // Adjust nano seconds to match expected result
  tc.nsec = 12345678;
  t.tv_nsec = 12345678;

  NdbTimestamp_GetAsString({buf[0], sizeof(buf[0])},
                           NdbTimestampStringFormat::Iso8601SystemTime, nullptr,
                           &tc);
  strftime(buf[1], sizeof(buf[1]), "%FT%T.012345%z", &tm);
  // Only compare up to GMT hour offset since format of GMT offset may differ
  // (+12:34 vs +1234)
  ok1(strncmp(buf[0], buf[1], 29) == 0);

  for (size_t i = 0; i < std::size(times); i++) {
    t.tv_sec = times[i].t;
    NdbTimestamp_GetAsString({buf[0], sizeof(buf[0])},
                             NdbTimestampStringFormat::Iso8601SystemTime, &t);
    int eq = (strcmp(buf[0], times[i].s[itz]) == 0);
    if (!eq) diag("ERROR: '%s' != '%s'", buf[0], times[i].s[itz]);
    ok1(eq);
  }
}

int main() {
  test_UTC();
  for (size_t i = 1; i < std::size(timezones); i++) test_TZ(i);
  return exit_status();
}

#endif
