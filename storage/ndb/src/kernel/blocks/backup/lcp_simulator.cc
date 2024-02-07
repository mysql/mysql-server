/*
   Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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
#include <stdio.h>
/**
 * LCP Simulation program
 * ----------------------
 * This program is a tool that can be used to simulate sizes of LCPs, the
 * total size of the LCPs that are stored on disk. For each LCP the program
 * can report number of parts in the checkpoint, size of this checkpoint, id
 * of LCP, percent of data that is in ALL pages and the total DB size.
 *
 * The size of the total LCP size is reported as a percentage of overhead
 * compared to the DB size.
 *
 * The method set_rates defines the update_rate, insert_rate and delete_rate
 * for each lcp_id. The idea is that this method can be changed to reflect
 * different simulation scenarios. The unit intended is MBytes. The current
 * implementation of set_rates starts with 100 LCP ids that are pure inserts
 * with 8 GByte added per LCP, thus after those 100 LCPs the DB size will be
 * 800 GByte.
 *
 * The next 100 LCPs are pure update load with the same size, 8 GByte per LCP.
 * The final 100 LCPs are pure delete loads that delete the entire DB.
 *
 * The method calculate_lcp_sizes implements the algorithm to calculate the
 * number of parts used in the checkpoint and the size of the checkpoint.
 * There are three parameters driving this calculation:
 * 1) recovery_work: This is the configuration parameter RecoveryWork.
 * 2) insert_work: This is the configuration parameter InsertRecoveryWork.
 * 3) delete_work: This is always set to 120 in 7.6.5.
 * The else part in this method implements the method used in 7.6.4.
 *
 * The idea with those parameters is to test different parameters and how they
 * affect the LCP size and the percentage overhead.
 *
 * The method calculate_num_lcps calculates the number of LCPs that are
 * required to create a restorable DB. The method calculate_total_lcp_size
 * calculates the total size of the LCPs on disk. The method update_db_size
 * calculates the DB size based on the insert_rate and delete_rate. Finally
 * the method calculate_overhead calculates the overhead in percentage based
 * on total LCP size and DB size.
 *
 * To run it with different parameters one needs to compile it again.
 * The simplest manner to compile and run it is the following:
 * shell> gcc lcp_simulator.cc
 * shell> ./a.out
 *
 * If you want to change some parameter, edit the program and compile and
 * run again.
 *
 * The print statement is surrounded by an if-statement that makes it possible
 * to select which LCP ids I am interested in looking at or any other
 * condition.
 */
static int calculate_num_lcps(long double *percent_size, int current_i) {
  long double sum_percent = percent_size[current_i - 1];
  int num_lcps = 1;
  long double comparator = 0.99999999;
  while (sum_percent < comparator && current_i > 0) {
    current_i--;
    sum_percent += percent_size[current_i - 1];
    num_lcps++;
  }
  return num_lcps;
}

static int calculate_total_lcp_size(long double *lcp_sizes, int current_i,
                                    int num_lcps) {
  long double total_lcp_size = 0.0;
  for (int i = 0; i < num_lcps; i++) {
    total_lcp_size += lcp_sizes[current_i - i - 1];
  }
  return (int)total_lcp_size;
}

static int db_size = 0; /* Initial db size, we assume MBytes as unit */
static long double update_rate;
static long double insert_rate;
static long double delete_rate;
static long double one_part = 1.0 / 2048.0;
static int num_parts = 0;

static void set_rates(int lcp_id) {
#define NUM_SIMULATED_LCPS 300
  if (lcp_id <= 100) {
    update_rate = 0.0;
    insert_rate = 8000.0;
    delete_rate = 0.0;
  } else if (lcp_id <= 200) {
    update_rate = 8000.0;
    insert_rate = 0.0;
    delete_rate = 0.0;
  } else {
    update_rate = 0.0;
    insert_rate = 0.0;
    delete_rate = 8000.0;
  }
}

static void calculate_lcp_sizes(long double &lcp_size,
                                long double &percent_size) {
  long double recovery_work = 60.0;
  long double insert_work = 45.0;
  long double delete_work = 120.0;
  if (1) {
    long double rate = update_rate;
    rate /= (long double)db_size;
    rate *= (long double)2048.0;
    rate *= (long double)100.0;
    rate /= (long double)recovery_work;

    long double del_rate = delete_rate;
    del_rate /= (long double)db_size;
    del_rate *= (long double)2048.0;
    del_rate *= delete_work;
    del_rate /= recovery_work;

    long double ins_rate = insert_rate;
    ins_rate /= (long double)db_size;
    ins_rate *= (long double)2048.0;
    ins_rate *= insert_work;
    ins_rate /= recovery_work;

    rate += ins_rate;
    rate += del_rate;

    num_parts = (int)rate;
    num_parts++;
    if (num_parts > 2048) num_parts = 2048;
    percent_size = (long double)num_parts / (long double)2048.0;
    lcp_size = percent_size * (long double)db_size;
    lcp_size += ((long double)1 - percent_size) * (insert_rate + update_rate);
  } else {
    long double rate = (update_rate + insert_rate + delete_rate);
    rate /= (long double)db_size;
    rate *= (long double)2048.0;
    rate *= (long double)100.0;
    rate /= (long double)recovery_work;
    num_parts = (int)rate;
    num_parts++;
    if (num_parts > 2048) num_parts = 2048;
    percent_size = (long double)num_parts / (long double)2048.0;
    lcp_size = percent_size * (long double)db_size;
    lcp_size += ((long double)1 - percent_size) * (insert_rate + update_rate);
  }
}

static int calculate_overhead(long long int total_lcp_size) {
  long double overhead;
  overhead = total_lcp_size;
  overhead /= db_size;
  overhead *= 100.0;
  overhead -= 100.0;
  return (int)overhead;
}

static void update_db_size() {
  db_size += (int)insert_rate;
  if (db_size > (int)delete_rate) {
    db_size -= (int)delete_rate;
  }
}

int main() {
  long double lcp_size;
  long double percent_size;
  int num_lcps;
  long long int total_lcp_size;
  int overhead_int;
  long double lcp_sizes[NUM_SIMULATED_LCPS + 1];
  long double percent_sizes[NUM_SIMULATED_LCPS + 1];

  percent_sizes[0] = 1.0;
  lcp_sizes[0] = 0.0;
  for (int i = 1; i <= NUM_SIMULATED_LCPS; i++) {
    set_rates(i);
    update_db_size();
    calculate_lcp_sizes(lcp_size, percent_size);

    lcp_sizes[i] = lcp_size;
    percent_sizes[i] = percent_size;
    num_lcps = calculate_num_lcps(&percent_sizes[0], i + 1);
    total_lcp_size = calculate_total_lcp_size(&lcp_sizes[0], i + 1, num_lcps);
    overhead_int = calculate_overhead(total_lcp_size);

    if (i % 1 == 0 || i < 20) {
      printf(
          "LCP %d: LCP size: %d MByte, NumParts: %d, Percent in LCP: %f,"
          " Num LCPs: %d, DB size = %d MByte, Total LCP size: %lld MBytes,"
          " Percent overhead: %d\n",
          i, (int)lcp_size, num_parts, (double)(100.0 * percent_size), num_lcps,
          db_size, total_lcp_size, overhead_int);
    }
  }
  return 0;
}
