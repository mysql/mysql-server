/*
   Copyright (c) 2026, Oracle and/or its affiliates.

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

#ifdef TEST_NDB_TRPMAN

#include "my_getopt.h"
#include "ndb_init.h"
#include "trpman.hpp"
#include "unittest/mytap/tap.h"
#include "util/ndb_opts.h"

static void print_utility_help();

bool opt_core;

static bool opt_print;
static bool opt_test;

static struct my_option my_long_options[] = {
    NdbStdOpt::usage,
    NdbStdOpt::help,
    NdbStdOpt::version,
    {"print", 'p', "print", &opt_print, nullptr, nullptr, GET_BOOL, NO_ARG, 0,
     0, 0, nullptr, 0, nullptr},
    {"test", 't', "test", &opt_test, nullptr, nullptr, GET_BOOL, NO_ARG, 1, 0,
     0, nullptr, 0, nullptr},
    NdbStdOpt::end_of_options};

static const char *load_defaults_groups[] = {"ndb_trpman-t", nullptr};

class TestTrpmanActivityHistogram {
 public:
  static void print_histogram_bin_limits(unsigned interval) {
    unsigned bin_limits[Trpman::TRP_ACTIVITY_HIST_BIN_COUNT];
    unsigned bin_count =
        Trpman::calculate_histogram_bin_limits(interval, bin_limits);

    printf("Histogram bins for interval %u, bin count %u:\n", interval,
           bin_count);
    for (unsigned i = 0; i < bin_count; i++) {
      unsigned width = bin_limits[i] - (i > 0 ? bin_limits[i - 1] : 0);
      printf("\tbin#%02d upper bound %u (width %u)\n", i, bin_limits[i], width);
    }
  }
  static void test_calculate_histogram_bin_limits(unsigned interval) {
    static constexpr unsigned min_bin_width = 2 * Trpman::TRP_TIME_SIGNAL_DELAY;
    unsigned bin_limits[Trpman::TRP_ACTIVITY_HIST_BIN_COUNT];

    unsigned bin_count =
        Trpman::calculate_histogram_bin_limits(interval, bin_limits);

    ok(bin_count >= 2,
       "Histogram should have at least 2 bins got %u (heartbeat interval %u)",
       bin_count, interval);

    if (bin_count >= 2)
      ok(5 * interval <= bin_limits[bin_count - 2],
         "The next last bin #%u (limit %u) should cover 5 heartbeat intervals "
         "(5x%u)",
         bin_count - 2, bin_limits[bin_count - 2], interval);

    if (bin_count >= 1)
      ok(bin_limits[bin_count - 1] == UINT_MAX,
         "The last bin #%u should be UINT_MAX(%u) got %u (heartbeat interval "
         "%u)",
         bin_count - 1, bin_limits[bin_count - 1], UINT_MAX, interval);

    unsigned prev_width = bin_limits[0];
    ok(prev_width == min_bin_width,
       "Bin %u (limit %u) width %u should have the minimal width %u (heartbeat "
       "interval %u)",
       0, bin_limits[0], prev_width, min_bin_width, interval);

    for (unsigned i = 1; i < bin_count; i++) {
      unsigned width = bin_limits[i] - bin_limits[i - 1];
      ok(prev_width <= width,
         "Bin %u (limit %u) width %u should not be narrower than previous bin "
         "(limit %u, width %u) (heartbeat interval %u)",
         i, bin_limits[i], width, bin_limits[i - 1], prev_width, interval);
      prev_width = width;
    }

    unsigned result =
        Trpman::verify_histogram(interval, {bin_limits, bin_count});
    ok(result == 0,
       "Trpman::verify_histogram should succeed with result = 0, got %u",
       result);
  }
};

int main(int argc, char *argv[]) {
  NDB_INIT(argv[0]);
  Ndb_opts opts(argc, argv, my_long_options, load_defaults_groups);
  opts.set_usage_funcs(print_utility_help);
  if (opts.handle_options()) {
    opts.usage();
    ndb_end(0);
    return 2;
  }

  if (argc == 0) {
    unsigned hb_intervals[] = {1,    17,   25,    75,    125,    170,
                               175,  199,  225,   275,   325,    350,
                               375,  400,  450,   500,   600,    999,
                               1200, 5000, 30000, 99999, 999999, 123456789};
    for (unsigned interval : hb_intervals) {
      if (opt_print)
        TestTrpmanActivityHistogram::print_histogram_bin_limits(interval);
      if (opt_test)
        TestTrpmanActivityHistogram::test_calculate_histogram_bin_limits(
            interval);
    }
    for (unsigned interval = 1; interval <= 1200; interval++) {
      if (opt_print)
        TestTrpmanActivityHistogram::print_histogram_bin_limits(interval);
      if (opt_test)
        TestTrpmanActivityHistogram::test_calculate_histogram_bin_limits(
            interval);
    }
  } else
    for (int argi = 0; argi < argc; argi++) {
      unsigned interval = atoi(argv[argi]);
      if (opt_print)
        TestTrpmanActivityHistogram::print_histogram_bin_limits(interval);
      if (opt_test)
        TestTrpmanActivityHistogram::test_calculate_histogram_bin_limits(
            interval);
    }
  return exit_status();
}

void print_utility_help() {
  printf("Usage: ndb_trpman-t [-p | --print] [-t | --test] interval ...\n");
}

#endif
