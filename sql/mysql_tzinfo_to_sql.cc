/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#include "my_alloc.h"              // MEM_ROOT
#include "my_io.h"                 // MY_FOPEN_BINARY
#include "mysql/psi/mysql_file.h"  // MYSQL_FILE
#include "print_version.h"
#include "sql/time_zone_common.h"
#include "sql/tzfile.h"                // TZ_MAX_REV_RANGES, tzhead
#include "welcome_copyright_notice.h"  // ORACLE_WELCOME_COPYRIGHT_NOTICE

/*
  Macro for reading 32-bit integer from network byte order (big-endian)
  from a unaligned memory location.
*/
#define int4net(A)                                                       \
  (int32)(((uint32)((uchar)(A)[3])) | (((uint32)((uchar)(A)[2])) << 8) | \
          (((uint32)((uchar)(A)[1])) << 16) |                            \
          (((uint32)((uchar)(A)[0])) << 24))

static const char *const MAGIC_STRING_FOR_INVALID_ZONEINFO_FILE =
    "Local time zone must be set--see zic manual page";

/*
  Load time zone description from zoneinfo (TZinfo) file.

  SYNOPSIS
    tz_load()
      name - path to zoneinfo file
      sp   - TIME_ZONE_INFO structure to fill

  RETURN VALUES
    0 - Ok
    1 - Error
*/
static bool tz_load(const char *name, TIME_ZONE_INFO *sp, MEM_ROOT *storage) {
  uchar *p;
  size_t read_from_file;
  uint i;
  MYSQL_FILE *file;

  if (!(file =
            mysql_file_fopen(0, name, O_RDONLY | MY_FOPEN_BINARY, MYF(MY_WME))))
    return true;
  {
    union {
      struct tzhead tzhead;
      uchar buf[sizeof(struct tzhead) + sizeof(my_time_t) * TZ_MAX_TIMES +
                TZ_MAX_TIMES + sizeof(TRAN_TYPE_INFO) * TZ_MAX_TYPES +
                std::max(TZ_MAX_CHARS + 1, (2 * (MY_TZNAME_MAX + 1))) +
                sizeof(LS_INFO) * TZ_MAX_LEAPS];
    } u;
    uint ttisstdcnt;
    uint ttisgmtcnt;
    char *tzinfo_buf;

    read_from_file = mysql_file_fread(file, u.buf, sizeof(u.buf), MYF(MY_WME));

    if (mysql_file_fclose(file, MYF(MY_WME)) != 0) return true;

    if (read_from_file < sizeof(struct tzhead)) return true;

    ttisstdcnt = int4net(u.tzhead.tzh_ttisgmtcnt);
    ttisgmtcnt = int4net(u.tzhead.tzh_ttisstdcnt);
    sp->leapcnt = int4net(u.tzhead.tzh_leapcnt);
    sp->timecnt = int4net(u.tzhead.tzh_timecnt);
    sp->typecnt = int4net(u.tzhead.tzh_typecnt);
    sp->charcnt = int4net(u.tzhead.tzh_charcnt);
    p = u.tzhead.tzh_charcnt + sizeof(u.tzhead.tzh_charcnt);
    if (sp->leapcnt > TZ_MAX_LEAPS || sp->typecnt == 0 ||
        sp->typecnt > TZ_MAX_TYPES || sp->timecnt > TZ_MAX_TIMES ||
        sp->charcnt > TZ_MAX_CHARS ||
        (ttisstdcnt != sp->typecnt && ttisstdcnt != 0) ||
        (ttisgmtcnt != sp->typecnt && ttisgmtcnt != 0))
      return true;
    if ((uint)(read_from_file - (p - u.buf)) <
        sp->timecnt * 4 +           /* ats */
            sp->timecnt +           /* types */
            sp->typecnt * (4 + 2) + /* ttinfos */
            sp->charcnt +           /* chars */
            sp->leapcnt * (4 + 4) + /* lsinfos */
            ttisstdcnt +            /* ttisstds */
            ttisgmtcnt)             /* ttisgmts */
      return true;

    size_t start_of_zone_abbrev = sizeof(struct tzhead) +
                                  sp->timecnt * 4 +      /* ats */
                                  sp->timecnt +          /* types */
                                  sp->typecnt * (4 + 2); /* ttinfos */

    /*
      Check that timezone file doesn't contain junk timezone data.
    */
    if (!memcmp(u.buf + start_of_zone_abbrev,
                MAGIC_STRING_FOR_INVALID_ZONEINFO_FILE,
                std::min(sizeof(MAGIC_STRING_FOR_INVALID_ZONEINFO_FILE) - 1,
                         sp->charcnt)))
      return true;

    size_t abbrs_buf_len = sp->charcnt + 1;

    if (!(tzinfo_buf = (char *)storage->Alloc(
              ALIGN_SIZE(sp->timecnt * sizeof(my_time_t)) +
              ALIGN_SIZE(sp->timecnt) +
              ALIGN_SIZE(sp->typecnt * sizeof(TRAN_TYPE_INFO)) +
              ALIGN_SIZE(abbrs_buf_len) + sp->leapcnt * sizeof(LS_INFO))))
      return true;

    sp->ats = (my_time_t *)tzinfo_buf;
    tzinfo_buf += ALIGN_SIZE(sp->timecnt * sizeof(my_time_t));
    sp->types = (uchar *)tzinfo_buf;
    tzinfo_buf += ALIGN_SIZE(sp->timecnt);
    sp->ttis = (TRAN_TYPE_INFO *)tzinfo_buf;
    tzinfo_buf += ALIGN_SIZE(sp->typecnt * sizeof(TRAN_TYPE_INFO));
    sp->chars = tzinfo_buf;
    tzinfo_buf += ALIGN_SIZE(abbrs_buf_len);
    sp->lsis = (LS_INFO *)tzinfo_buf;

    for (i = 0; i < sp->timecnt; i++, p += 4) sp->ats[i] = int4net(p);

    for (i = 0; i < sp->timecnt; i++) {
      sp->types[i] = *p++;
      if (sp->types[i] >= sp->typecnt) return true;
    }
    for (i = 0; i < sp->typecnt; i++) {
      TRAN_TYPE_INFO *ttisp;

      ttisp = &sp->ttis[i];
      ttisp->tt_gmtoff = int4net(p);
      p += 4;
      ttisp->tt_isdst = *p++;
      if (ttisp->tt_isdst != 0 && ttisp->tt_isdst != 1) return true;
      ttisp->tt_abbrind = *p++;
      if (ttisp->tt_abbrind > sp->charcnt) return true;
    }
    for (i = 0; i < sp->charcnt; i++) sp->chars[i] = *p++;
    sp->chars[i] = '\0'; /* ensure '\0' at end */
    for (i = 0; i < sp->leapcnt; i++) {
      LS_INFO *lsisp;

      lsisp = &sp->lsis[i];
      lsisp->ls_trans = int4net(p);
      p += 4;
      lsisp->ls_corr = int4net(p);
      p += 4;
    }
    /*
      Since we don't support POSIX style TZ definitions in variables we
      don't read further like glibc or elsie code.
    */
  }

  return prepare_tz_info(sp, storage);
}

/*
  This code belongs to mysql_tzinfo_to_sql converter command line utility.
  This utility should be used by db admin for populating mysql.time_zone
  tables.
*/

/*
  Print info about time zone described by TIME_ZONE_INFO struct as
  SQL statements populating mysql.time_zone* tables.

  SYNOPSIS
    print_tz_as_sql()
      tz_name - name of time zone
      sp      - structure describing time zone
*/
static void print_tz_as_sql(const char *tz_name, const TIME_ZONE_INFO *sp) {
  uint i;

  /* Here we assume that all time zones have same leap correction tables */
  printf("INSERT INTO time_zone (Use_leap_seconds) VALUES ('%s');\n",
         sp->leapcnt ? "Y" : "N");
  printf("SET @time_zone_id= LAST_INSERT_ID();\n");
  printf(
      "INSERT INTO time_zone_name (Name, Time_zone_id) VALUES \
('%s', @time_zone_id);\n",
      tz_name);

  if (sp->timecnt) {
    printf(
        "INSERT INTO time_zone_transition \
(Time_zone_id, Transition_time, Transition_type_id) VALUES\n");
    for (i = 0; i < sp->timecnt; i++)
      printf("%s(@time_zone_id, %lld, %u)\n", (i == 0 ? " " : ","),
             static_cast<long long int>(sp->ats[i]),
             static_cast<uint>(sp->types[i]));
    printf(";\n");
  }

  printf(
      "INSERT INTO time_zone_transition_type \
(Time_zone_id, Transition_type_id, Offset, Is_DST, Abbreviation) VALUES\n");

  for (i = 0; i < sp->typecnt; i++)
    /*
      Since the column time_zone_transition_type.Abbreviation
      is declared as CHAR(8) we have to limit the number of characters
      for the column abbreviation in the next output by 8 chars.
    */
    printf("%s(@time_zone_id, %u, %ld, %d, '%.8s')\n", (i == 0 ? " " : ","), i,
           sp->ttis[i].tt_gmtoff, sp->ttis[i].tt_isdst,
           sp->chars + sp->ttis[i].tt_abbrind);
  printf(";\n");
}

/*
  Print info about leap seconds in time zone as SQL statements
  populating mysql.time_zone_leap_second table.

  SYNOPSIS
    print_tz_leaps_as_sql()
      sp      - structure describing time zone
*/
static void print_tz_leaps_as_sql(const TIME_ZONE_INFO *sp) {
  uint i;

  /*
    We are assuming that there are only one list of leap seconds
    For all timezones.
  */
  printf("TRUNCATE TABLE time_zone_leap_second;\n");
  printf("START TRANSACTION;\n");
  if (sp->leapcnt) {
    printf(
        "INSERT INTO time_zone_leap_second \
(Transition_time, Correction) VALUES\n");
    for (i = 0; i < sp->leapcnt; i++)
      printf("%s(%lld, %ld)\n", (i == 0 ? " " : ","),
             static_cast<long long int>(sp->lsis[i].ls_trans),
             sp->lsis[i].ls_corr);
    printf(";\n");
  }
  printf("COMMIT;\n");
}

/*
  Some variables used as temporary or as parameters
  in recursive scan_tz_dir() code.
*/
TIME_ZONE_INFO tz_info;
MEM_ROOT tz_storage;
char fullname[FN_REFLEN + 1];
char *root_name_end;

/*
  Recursively scan zoneinfo directory and print all found time zone
  descriptions as SQL.

  SYNOPSIS
    scan_tz_dir()
      name_end - pointer to end of path to directory to be searched.

  DESCRIPTION
    This auxiliary recursive function also uses several global
    variables as in parameters and for storing temporary values.

    fullname      - path to directory that should be scanned.
    root_name_end - pointer to place in fullname where part with
                    path to initial directory ends.
    current_tz_id - last used time zone id

  RETURN VALUE
    0 - Ok, 1 - Fatal error

*/
static bool scan_tz_dir(char *name_end) {
  MY_DIR *cur_dir;
  char *name_end_tmp;
  uint i;

  if (!(cur_dir = my_dir(fullname, MYF(MY_WANT_STAT)))) return true;

  name_end = strmake(name_end, "/", FN_REFLEN - (name_end - fullname));

  for (i = 0; i < cur_dir->number_off_files; i++) {
    if (cur_dir->dir_entry[i].name[0] != '.') {
      name_end_tmp = strmake(name_end, cur_dir->dir_entry[i].name,
                             FN_REFLEN - (name_end - fullname));

      if (MY_S_ISDIR(cur_dir->dir_entry[i].mystat->st_mode)) {
        if (scan_tz_dir(name_end_tmp)) {
          my_dirend(cur_dir);
          return true;
        }
      } else if (MY_S_ISREG(cur_dir->dir_entry[i].mystat->st_mode)) {
        ::new ((void *)&tz_storage) MEM_ROOT(PSI_NOT_INSTRUMENTED, 32768);
        if (!tz_load(fullname, &tz_info, &tz_storage))
          print_tz_as_sql(root_name_end + 1, &tz_info);
        else
          fprintf(stderr,
                  "Warning: Unable to load '%s' as time zone. Skipping it.\n",
                  fullname);
        tz_storage.Clear();
      } else
        fprintf(stderr, "Warning: '%s' is not regular file or directory\n",
                fullname);
    }
  }

  my_dirend(cur_dir);

  return false;
}

extern "C" void sql_alloc_error_handler() {}

int main(int argc, char **argv) {
  MY_INIT(argv[0]);

  if (argc != 2 && argc != 3) {
    print_version();
    puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2004"));
    fprintf(stderr, "Usage:\n");
    fprintf(stderr, " %s timezonedir\n", argv[0]);
    fprintf(stderr, " %s timezonefile timezonename\n", argv[0]);
    fprintf(stderr, " %s --leap timezonefile\n", argv[0]);
    return 1;
  }

  if (argc == 2) {
    root_name_end = strmake(fullname, argv[1], FN_REFLEN);

    printf("TRUNCATE TABLE time_zone;\n");
    printf("TRUNCATE TABLE time_zone_name;\n");
    printf("TRUNCATE TABLE time_zone_transition;\n");
    printf("TRUNCATE TABLE time_zone_transition_type;\n");

    printf("START TRANSACTION;\n");
    if (scan_tz_dir(root_name_end)) {
      fprintf(stderr,
              "There were fatal errors during processing "
              "of zoneinfo directory\n");
      return 1;
    }
    printf("COMMIT;\n");
  } else {
    ::new ((void *)&tz_storage) MEM_ROOT(PSI_NOT_INSTRUMENTED, 32768);

    if (strcmp(argv[1], "--leap") == 0) {
      if (tz_load(argv[2], &tz_info, &tz_storage)) {
        fprintf(stderr, "Problems with zoneinfo file '%s'\n", argv[2]);
        return 1;
      }
      print_tz_leaps_as_sql(&tz_info);
    } else {
      if (tz_load(argv[1], &tz_info, &tz_storage)) {
        fprintf(stderr, "Problems with zoneinfo file '%s'\n", argv[2]);
        return 1;
      }
      printf("START TRANSACTION;\n");
      print_tz_as_sql(argv[2], &tz_info);
      printf("COMMIT;\n");
    }

    tz_storage.Clear();
  }

  return 0;
}
