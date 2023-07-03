/* Copyright (c) 2001, 2023, Oracle and/or its affiliates.

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

/* Written by Sergei A. Golubchik, who has a shared copyright to this code
   added support for long options (my_getopt) 22.5.2002 by Jani Tolonen */

#include <fcntl.h>
#include <stdlib.h>
#include <sys/types.h>

#include "my_compiler.h"
#include "my_getopt.h"
#include "my_inttypes.h"
#include "print_version.h"
#include "storage/myisam/ftdefs.h"
#include "storage/myisam/myisamdef.h"
#include "welcome_copyright_notice.h"

[[noreturn]] static void usage();
static void complain(int val);
static bool get_one_option(int, const struct my_option *, char *);

static int count = 0, stats = 0, dump = 0, lstats = 0;
static bool verbose;
static char *query = nullptr;
static uint lengths[256];

#define MAX_LEN (HA_FT_MAXBYTELEN + 10)
#define HOW_OFTEN_TO_WRITE 10000

static struct my_option my_long_options[] = {
    {"help", 'h', "Display help and exit.", nullptr, nullptr, nullptr,
     GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"help", '?', "Synonym for -h.", nullptr, nullptr, nullptr, GET_NO_ARG,
     NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"count", 'c', "Calculate per-word stats (counts and global weights).",
     nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0,
     nullptr},
    {"dump", 'd', "Dump index (incl. data offsets and word weights).", nullptr,
     nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"length", 'l', "Report length distribution.", nullptr, nullptr, nullptr,
     GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"stats", 's', "Report global stats.", nullptr, nullptr, nullptr,
     GET_NO_ARG, NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {"verbose", 'v', "Be verbose.", &verbose, &verbose, nullptr, GET_BOOL,
     NO_ARG, 0, 0, 0, nullptr, 0, nullptr},
    {nullptr, 0, nullptr, nullptr, nullptr, nullptr, GET_NO_ARG, NO_ARG, 0, 0,
     0, nullptr, 0, nullptr}};

extern st_keycache_thread_var *keycache_thread_var() {
  return &main_thread_keycache_var;
}

int main(int argc, char *argv[]) {
  int error = 0, subkeys;
  uint keylen, keylen2 = 0, inx, doc_cnt = 0;
  float weight = 1.0;
  double gws, min_gws = 0, avg_gws = 0;
  MI_INFO *info;
  char buf[MAX_LEN], buf2[MAX_LEN], buf_maxlen[MAX_LEN], buf_min_gws[MAX_LEN];
  ulong total = 0, maxlen = 0, uniq = 0, max_doc_cnt = 0;
  struct {
    MI_INFO *info;
  } aio0, *aio = &aio0; /* for GWS_IN_USE */

  MY_INIT(argv[0]);

  memset(&main_thread_keycache_var, 0, sizeof(st_keycache_thread_var));
  mysql_cond_init(PSI_NOT_INSTRUMENTED, &main_thread_keycache_var.suspend);

  if ((error = handle_options(&argc, &argv, my_long_options, get_one_option)))
    exit(error);
  if (count || dump) verbose = false;
  if (!count && !dump && !lstats && !query) stats = 1;

  if (verbose) setbuf(stdout, nullptr);

  if (argc < 2) usage();

  {
    char *end;
    inx = (uint)my_strtoll(argv[1], &end, 10);
    if (*end) usage();
  }

  init_key_cache(dflt_key_cache, MI_KEY_BLOCK_LENGTH, USE_BUFFER_INIT, 0, 0);

  if (!(info = mi_open(argv[0], O_RDONLY,
                       HA_OPEN_ABORT_IF_LOCKED | HA_OPEN_FROM_SQL_LAYER))) {
    error = my_errno();
    goto err;
  }

  *buf2 = 0;
  aio->info = info;

  if ((inx >= info->s->base.keys) ||
      !(info->s->keyinfo[inx].flag & HA_FULLTEXT)) {
    printf("Key %d in table %s is not a FULLTEXT key\n", inx, info->filename);
    goto err;
  }

  mi_lock_database(info, F_EXTRA_LCK);

  info->lastpos = HA_OFFSET_ERROR;
  info->update |= HA_STATE_PREV_FOUND;

  while (!(error = mi_rnext(info, nullptr, inx))) {
    keylen = *(info->lastkey);

    subkeys = ft_sintXkorr(info->lastkey + keylen + 1);
    if (subkeys >= 0) weight = ft_floatXget(info->lastkey + keylen + 1);

    snprintf(buf, MAX_LEN, "%.*s", (int)keylen, info->lastkey + 1);
    my_casedn_str(default_charset_info, buf);
    total++;
    lengths[keylen]++;

    if (count || stats) {
      if (strcmp(buf, buf2)) {
        if (*buf2) {
          uniq++;
          avg_gws += gws = GWS_IN_USE;
          if (count) printf("%9u %20.7f %s\n", doc_cnt, gws, buf2);
          if (maxlen < keylen2) {
            maxlen = keylen2;
            my_stpcpy(buf_maxlen, buf2);
          }
          if (max_doc_cnt < doc_cnt) {
            max_doc_cnt = doc_cnt;
            my_stpcpy(buf_min_gws, buf2);
            min_gws = gws;
          }
        }
        my_stpcpy(buf2, buf);
        keylen2 = keylen;
        doc_cnt = 0;
      }
      doc_cnt += (subkeys >= 0 ? 1 : -subkeys);
    }
    if (dump) {
      if (subkeys >= 0)
        printf("%9lx %20.7f %s\n", (long)info->lastpos, weight, buf);
      else
        printf("%9lx => %17d %s\n", (long)info->lastpos, -subkeys, buf);
    }
    if (verbose && (total % HOW_OFTEN_TO_WRITE) == 0) printf("%10ld\r", total);
  }
  mi_lock_database(info, F_UNLCK);

  if (count || stats) {
    if (*buf2) {
      uniq++;
      avg_gws += gws = GWS_IN_USE;
      if (count) printf("%9u %20.7f %s\n", doc_cnt, gws, buf2);
      if (maxlen < keylen2) {
        maxlen = keylen2;
        my_stpcpy(buf_maxlen, buf2);
      }
      if (max_doc_cnt < doc_cnt) {
        max_doc_cnt = doc_cnt;
        my_stpcpy(buf_min_gws, buf2);
        min_gws = gws;
      }
    }
  }

  if (stats) {
    count = 0;
    for (inx = 0; inx < 256; inx++) {
      count += lengths[inx];
      if ((ulong)count >= total / 2) break;
    }
    printf(
        "Total rows: %lu\nTotal words: %lu\n"
        "Unique words: %lu\nLongest word: %lu chars (%s)\n"
        "Median length: %u\n"
        "Average global weight: %f\n"
        "Most common word: %lu times, weight: %f (%s)\n",
        (long)info->state->records, total, uniq, maxlen, buf_maxlen, inx,
        avg_gws / uniq, max_doc_cnt, min_gws, buf_min_gws);
  }
  if (lstats) {
    count = 0;
    for (inx = 0; inx < 256; inx++) {
      count += lengths[inx];
      if (count && lengths[inx])
        printf("%3u: %10lu %5.2f%% %20lu %4.1f%%\n", inx, (ulong)lengths[inx],
               100.0 * lengths[inx] / total, (ulong)count,
               100.0 * count / total);
    }
  }

err:
  if (error && error != HA_ERR_END_OF_FILE)
    printf("got error %d\n", my_errno());
  if (info) mi_close(info);
  mysql_cond_destroy(&main_thread_keycache_var.suspend);
  return 0;
}

static bool get_one_option(int optid,
                           const struct my_option *opt [[maybe_unused]],
                           char *argument [[maybe_unused]]) {
  switch (optid) {
    case 'd':
      dump = 1;
      complain(count || query);
      break;
    case 's':
      stats = 1;
      complain(query != nullptr);
      break;
    case 'c':
      count = 1;
      complain(dump || query);
      break;
    case 'l':
      lstats = 1;
      complain(query != nullptr);
      break;
    case '?':
    case 'h':
      usage();
  }
  return false;
}

static void usage() {
  print_version();
  puts(ORACLE_WELCOME_COPYRIGHT_NOTICE("2002"));

  printf("Use: myisam_ftdump <table_name> <index_num>\n");
  my_print_help(my_long_options);
  my_print_variables(my_long_options);
  exit(1);
}

static void complain(int val) /* Kinda assert :-)  */
{
  if (val) {
    printf("You cannot use these options together!\n");
    exit(1);
  }
}

#include "storage/myisam/mi_extrafunc.h"
