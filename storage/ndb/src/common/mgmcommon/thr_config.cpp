/*
   Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#include "thr_config.hpp"
#include <NdbThread.h>
#include <kernel/ndb_limits.h>
#include <EventLogger.hpp>
#include <NdbHW.hpp>
#include "../../common/util/parse_mask.hpp"
#include "util/require.h"

#if (defined(VM_TRACE) || defined(ERROR_INSERT))
#define DEBUG_AUTO_THREAD_CONFIG 1
#endif

#ifdef DEBUG_AUTO_THREAD_CONFIG
#define DEB_AUTO_THREAD_CONFIG(arglist) \
  do {                                  \
    g_eventLogger->info arglist;        \
  } while (0)
#else
#define DEB_AUTO_THREAD_CONFIG(arglist) \
  do {                                  \
  } while (0)
#endif

static const struct ParseEntries m_parse_entries[] = {
    // name     type
    {"main", THRConfig::T_MAIN},      {"ldm", THRConfig::T_LDM},
    {"recv", THRConfig::T_RECV},      {"rep", THRConfig::T_REP},
    {"io", THRConfig::T_IO},          {"watchdog", THRConfig::T_WD},
    {"tc", THRConfig::T_TC},          {"send", THRConfig::T_SEND},
    {"idxbld", THRConfig::T_IXBLD},   {"query", THRConfig::T_QUERY},
    {"recover", THRConfig::T_RECOVER}};

/**
 * The min and max values for T_IO (IO threads) and T_WD (watchdog threads)
 * will always be 1, thus count must always be set to 1. These threads
 * ignore the count setting but since ThreadConfig is designed around
 * setting thread counts, it still needs to be set. The number of IO
 * threads and watchdog threads is handled without configuration.
 *
 * There are other properties such as thread priority that can be set on those
 * thread types.
 */
static const struct THRConfig::Entries m_entries[] = {
    // type                min max                       exec thread   permanent
    // default_count
    {THRConfig::T_MAIN, 0, 1, true, true, 1},
    {THRConfig::T_LDM, 0, MAX_NDBMT_LQH_THREADS, true, true, 1},
    {THRConfig::T_RECV, 1, MAX_NDBMT_RECEIVE_THREADS, true, true, 1},
    {THRConfig::T_REP, 0, 1, true, true, 1},
    {THRConfig::T_IO, 1, 1, false, true, 1},
    {THRConfig::T_WD, 1, 1, false, true, 1},
    {THRConfig::T_TC, 0, MAX_NDBMT_TC_THREADS, true, true, 0},
    {THRConfig::T_SEND, 0, MAX_NDBMT_SEND_THREADS, true, true, 0},
    {THRConfig::T_IXBLD, 0, 1, false, false, 0},
    {THRConfig::T_QUERY, 0, MAX_NDBMT_QUERY_THREADS, true, true, 0},
    {THRConfig::T_RECOVER, 0, MAX_NDBMT_QUERY_THREADS, false, false, 0}};

static const struct ParseParams m_params[] = {
    {"count", ParseParams::S_UNSIGNED},
    {"cpubind", ParseParams::S_BITMASK},
    {"cpubind_exclusive", ParseParams::S_BITMASK},
    {"cpuset", ParseParams::S_BITMASK},
    {"cpuset_exclusive", ParseParams::S_BITMASK},
    {"realtime", ParseParams::S_UNSIGNED},
    {"spintime", ParseParams::S_UNSIGNED},
    {"thread_prio", ParseParams::S_UNSIGNED},
    {"nosend", ParseParams::S_UNSIGNED}};

#define IX_COUNT 0
#define IX_CPUBIND 1
#define IX_CPUBIND_EXCLUSIVE 2
#define IX_CPUSET 3
#define IX_CPUSET_EXCLUSIVE 4
#define IX_REALTIME 5
#define IX_SPINTIME 6
#define IX_THREAD_PRIO 7
#define IX_NOSEND 8

unsigned THRConfig::getMaxEntries(Uint32 type) {
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_entries); i++) {
    if (m_entries[i].m_type == type) return m_entries[i].m_max_cnt;
  }
  return 0;
}

unsigned THRConfig::getMinEntries(Uint32 type) {
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_entries); i++) {
    if (m_entries[i].m_type == type) return m_entries[i].m_min_cnt;
  }
  return 0;
}

const char *THRConfig::getEntryName(Uint32 type) {
  for (unsigned int i = 0; i < NDB_ARRAY_SIZE(m_parse_entries); i++) {
    if (m_parse_entries[i].m_type == type) {
      return m_parse_entries[i].m_name;
    }
  }
  return nullptr;
}

THRConfig::THRConfig() { m_classic = false; }

THRConfig::~THRConfig() {}

int THRConfig::setLockExecuteThreadToCPU(const char *mask) {
  int res = parse_mask(mask, m_LockExecuteThreadToCPU);
  if (res < 0) {
    m_err_msg.assfmt(
        "failed to parse 'LockExecuteThreadToCPU=%s' "
        "(error: %d)",
        mask, res);
    return -1;
  } else if (res == 0) {
    m_err_msg.assfmt(
        "LockExecuteThreadToCPU: %s"
        " with empty bitmask not allowed",
        mask);
    return -1;
  }
  return 0;
}

int THRConfig::setLockIoThreadsToCPU(unsigned val) {
  m_LockIoThreadsToCPU.set(val);
  return 0;
}

void THRConfig::add(T_Type t, unsigned realtime, unsigned spintime) {
  T_Thread tmp;
  tmp.m_type = t;
  tmp.m_bind_type = T_Thread::B_UNBOUND;
  tmp.m_no = m_threads[t].size();
  tmp.m_realtime = realtime;
  tmp.m_thread_prio = NO_THREAD_PRIO_USED;
  tmp.m_nosend = 0;
  if (spintime > 9000) spintime = 9000;
  tmp.m_spintime = spintime;
  tmp.m_core_bind = false;
  m_threads[t].push_back(tmp);
}

static void computeThreadConfig(Uint32 MaxNoOfExecutionThreads,
                                Uint32 &tcthreads, Uint32 &lqhthreads,
                                Uint32 &sendthreads, Uint32 &recvthreads) {
  assert(MaxNoOfExecutionThreads >= 9);
  static const struct entry {
    Uint32 M;
    Uint32 lqh;
    Uint32 tc;
    Uint32 send;
    Uint32 recv;
  } table[] = {{9, 4, 2, 0, 1},    {10, 4, 2, 1, 1},   {11, 4, 3, 1, 1},
               {12, 6, 2, 1, 1},   {13, 6, 3, 1, 1},   {14, 6, 3, 1, 2},
               {15, 6, 3, 2, 2},   {16, 8, 3, 1, 2},   {17, 8, 4, 1, 2},
               {18, 8, 4, 2, 2},   {19, 8, 5, 2, 2},   {20, 10, 4, 2, 2},
               {21, 10, 5, 2, 2},  {22, 10, 5, 2, 3},  {23, 10, 6, 2, 3},
               {24, 12, 5, 2, 3},  {25, 12, 6, 2, 3},  {26, 12, 6, 3, 3},
               {27, 12, 7, 3, 3},  {28, 12, 7, 3, 4},  {29, 12, 8, 3, 4},
               {30, 12, 8, 4, 4},  {31, 12, 9, 4, 4},  {32, 16, 7, 4, 3},
               {33, 16, 7, 4, 4},  {34, 16, 8, 4, 4},  {35, 16, 9, 4, 4},
               {36, 16, 10, 4, 4}, {37, 16, 10, 4, 5}, {38, 16, 11, 4, 5},
               {39, 16, 12, 4, 5}, {40, 20, 10, 4, 4}, {41, 20, 10, 4, 5},
               {42, 20, 11, 4, 5}, {43, 20, 11, 5, 5}, {44, 20, 12, 5, 5},
               {45, 20, 12, 5, 6}, {46, 20, 13, 5, 6}, {47, 20, 14, 5, 6},
               {48, 24, 11, 6, 5}, {49, 24, 11, 6, 6}, {50, 24, 12, 6, 6},
               {51, 24, 13, 6, 6}, {52, 24, 14, 6, 6}, {53, 24, 14, 6, 7},
               {54, 24, 15, 6, 7}, {55, 24, 16, 6, 7}, {56, 24, 17, 6, 7},
               {57, 24, 18, 6, 8}, {58, 24, 19, 6, 8}, {59, 24, 17, 8, 8},
               {60, 24, 18, 8, 8}, {61, 24, 18, 8, 9}, {62, 24, 19, 8, 9},
               {63, 24, 19, 9, 9}, {64, 32, 15, 8, 7}, {65, 32, 15, 8, 8},
               {66, 32, 16, 8, 8}, {67, 32, 17, 8, 8}, {68, 32, 18, 8, 8},
               {69, 32, 18, 8, 9}, {70, 32, 19, 8, 9}, {71, 32, 20, 8, 9},
               {72, 32, 20, 8, 10}};

  Uint32 P = MaxNoOfExecutionThreads - 9;
  if (P >= NDB_ARRAY_SIZE(table)) {
    P = NDB_ARRAY_SIZE(table) - 1;
  }

  lqhthreads = table[P].lqh;
  tcthreads = table[P].tc;
  sendthreads = table[P].send;
  recvthreads = table[P].recv;
}

static Uint32 g_num_query_threads_per_ldm = 0;

void THRConfig::compute_automatic_thread_config(
    Uint32 num_cpus, Uint32 &tc_threads, Uint32 &ldm_threads,
    Uint32 &query_threads, Uint32 &recover_threads, Uint32 &main_threads,
    Uint32 &rep_threads, Uint32 &send_threads, Uint32 &recv_threads) {
  static const struct map_entry {
    Uint32 cpu_cnt;
    Uint32 mapped_id;
  } map_table[] = {
      {1, 0},    {2, 1},    {3, 1},    {4, 2},    {5, 2},    {6, 3},
      {7, 3},    {8, 4},    {9, 4},    {10, 5},   {11, 5},   {12, 6},
      {13, 6},   {14, 7},   {15, 7},   {16, 8},   {17, 8},   {18, 9},
      {19, 9},   {20, 10},  {21, 10},  {22, 11},  {23, 11},  {24, 12},
      {25, 12},  {26, 13},  {27, 13},  {28, 14},  {29, 14},  {30, 15},
      {31, 15},  {32, 16},  {33, 16},  {34, 16},  {35, 16},  {36, 17},
      {37, 17},  {38, 17},  {39, 17},  {40, 18},  {41, 18},  {42, 18},
      {43, 18},  {44, 19},  {45, 19},  {46, 19},  {47, 19},  {48, 20},
      {49, 20},  {50, 20},  {51, 20},  {52, 21},  {53, 21},  {54, 21},
      {55, 21},  {56, 22},  {57, 22},  {58, 22},  {59, 22},  {60, 23},
      {61, 23},  {62, 23},  {63, 23},  {64, 24},  {65, 24},  {66, 24},
      {67, 24},  {68, 24},  {69, 24},  {70, 24},  {71, 24},  {72, 25},
      {73, 25},  {74, 25},  {75, 25},  {76, 25},  {77, 25},  {78, 25},
      {79, 25},  {80, 26},  {81, 26},  {82, 26},  {83, 26},  {84, 26},
      {85, 26},  {86, 26},  {87, 26},  {88, 27},  {89, 27},  {90, 27},
      {91, 27},  {92, 27},  {93, 27},  {94, 27},  {95, 27},  {96, 28},
      {97, 28},  {98, 28},  {99, 28},  {100, 28}, {101, 28}, {102, 28},
      {103, 28}, {104, 29}, {105, 29}, {106, 29}, {107, 29}, {108, 29},
      {109, 29}, {110, 29}, {111, 29}, {112, 30}, {113, 30}, {114, 30},
      {115, 30}, {116, 30}, {117, 30}, {118, 30}, {119, 30}, {120, 31},
      {121, 31}, {122, 31}, {123, 31}, {124, 31}, {125, 31}, {126, 31},
      {127, 31}, {128, 32}, {129, 32}, {130, 32}, {131, 32}, {132, 32},
      {133, 32}, {134, 32}, {135, 32}, {136, 32}, {137, 32}, {138, 32},
      {139, 32}, {140, 33}, {141, 33}, {142, 33}, {143, 33}, {144, 33},
      {145, 33}, {146, 33}, {147, 33}, {148, 33}, {149, 33}, {150, 33},
      {151, 33}, {152, 34}, {153, 34}, {154, 34}, {155, 34}, {156, 34},
      {157, 34}, {158, 34}, {159, 34}, {160, 34}, {161, 34}, {162, 34},
      {163, 34}, {164, 35}, {165, 35}, {166, 35}, {167, 35}, {168, 35},
      {169, 35}, {170, 35}, {171, 35}, {172, 35}, {173, 35}, {174, 35},
      {175, 35}, {176, 36}, {177, 36}, {178, 36}, {179, 36}, {180, 36},
      {181, 36}, {182, 36}, {183, 36}, {184, 36}, {185, 36}, {186, 36},
      {187, 36}, {188, 37}, {189, 37}, {190, 37}, {191, 37}, {192, 37},
      {193, 37}, {194, 37}, {195, 37}, {196, 37}, {197, 37}, {198, 37},
      {199, 37}, {200, 38}, {201, 38}, {202, 38}, {203, 38}, {204, 38},
      {205, 38}, {206, 38}, {207, 38}, {208, 38}, {209, 38}, {210, 38},
      {211, 38}, {212, 39}, {213, 39}, {214, 39}, {215, 39}, {216, 39},
      {217, 39}, {218, 39}, {219, 39}, {220, 39}, {221, 39}, {222, 39},
      {223, 39}, {224, 40}, {225, 40}, {226, 40}, {227, 40}, {228, 40},
      {229, 40}, {230, 40}, {231, 40}, {232, 40}, {233, 40}, {234, 40},
      {235, 40}, {236, 40}, {237, 40}, {238, 40}, {239, 40}, {240, 41},
      {241, 41}, {242, 41}, {243, 41}, {244, 41}, {245, 41}, {246, 41},
      {247, 41}, {248, 41}, {249, 41}, {250, 41}, {251, 41}, {252, 41},
      {253, 41}, {254, 41}, {255, 41}};

  static const struct entry {
    Uint32 map_id;
    Uint32 main_threads;
    Uint32 rep_threads;
    Uint32 ldm_threads;
    Uint32 query_threads;
    Uint32 tc_threads;
    Uint32 send_threads;
    Uint32 recv_threads;
  } table[] = {
      {0, 0, 0, 0, 0, 0, 0, 1},           // 1 CPU
      {1, 1, 0, 0, 0, 0, 0, 1},           // 2-3 CPUs
      {2, 1, 0, 1, 1, 0, 0, 1},           // 4-5 CPUs
      {3, 1, 0, 2, 2, 0, 0, 1},           // 6-7 CPUs
      {4, 1, 0, 2, 2, 1, 1, 1},           // 8-9 CPUs
      {5, 1, 0, 3, 3, 1, 1, 1},           // 10-11 CPUs
      {6, 1, 1, 3, 3, 2, 1, 1},           // 12-13 CPUs
      {7, 1, 1, 4, 4, 2, 1, 1},           // 14-15 CPUs
      {8, 1, 1, 4, 4, 2, 2, 2},           // 16-17 CPUs
      {9, 1, 1, 5, 5, 2, 2, 2},           // 18-19 CPUs
      {10, 1, 1, 5, 5, 3, 2, 3},          // 20-21 CPUs
      {11, 1, 1, 6, 6, 3, 2, 3},          // 22-23 CPUs
      {12, 1, 1, 6, 6, 4, 3, 3},          // 24-25 CPUs
      {13, 1, 1, 7, 7, 4, 3, 3},          // 26-27 CPUs
      {14, 1, 1, 8, 8, 4, 3, 3},          // 28-29 CPUs
      {15, 1, 1, 8, 8, 5, 3, 4},          // 30-31 CPUs
      {16, 1, 1, 9, 9, 5, 3, 4},          // 32-35 CPUs
      {17, 1, 1, 10, 10, 6, 4, 4},        // 36-39 CPUs
      {18, 1, 1, 12, 12, 6, 4, 4},        // 40-43 CPUs
      {19, 1, 1, 13, 13, 8, 4, 4},        // 44-47 CPUs
      {20, 1, 1, 14, 14, 8, 5, 5},        // 48-51 CPUs
      {21, 1, 1, 16, 16, 8, 5, 5},        // 52-55 CPUs
      {22, 1, 1, 17, 17, 9, 5, 6},        // 56-59 CPUs
      {23, 1, 1, 19, 19, 9, 5, 6},        // 60-63 CPUs
      {24, 1, 1, 20, 20, 10, 6, 6},       // 64-71 CPUs
      {25, 1, 1, 22, 22, 12, 7, 7},       // 72-79 CPUs
      {26, 1, 1, 25, 25, 12, 8, 8},       // 80-87 CPUs
      {27, 1, 1, 27, 27, 14, 9, 9},       // 88-95 CPUs
      {28, 1, 1, 30, 30, 15, 9, 10},      // 96-103 CPUs
      {29, 1, 1, 33, 33, 16, 10, 10},     // 104-111 CPUs
      {30, 1, 1, 36, 36, 17, 10, 11},     // 112-119 CPUs
      {31, 1, 1, 39, 39, 18, 11, 11},     // 120-127 CPUs
      {32, 1, 1, 42, 42, 19, 11, 12},     // 128-139 CPUs
      {33, 1, 1, 46, 46, 21, 12, 13},     // 140-151 CPUs
      {34, 1, 1, 50, 50, 23, 13, 14},     // 152-163 CPUs
      {35, 1, 1, 54, 54, 25, 14, 15},     // 164-175 CPUs
      {36, 1, 1, 58, 58, 27, 15, 16},     // 176-187 CPUs
      {37, 1, 1, 62, 62, 29, 16, 17},     // 188-199 CPUs
      {38, 1, 1, 66, 66, 31, 17, 18},     // 200-211 CPUs
      {39, 1, 1, 70, 70, 33, 18, 19},     // 212-223 CPUs
      {40, 1, 1, 74, 74, 35, 19, 20},     // 224-239 CPUs
      {41, 1, 1, 80, 80, 37, 20, 21},     // 240-255 CPUs
      {42, 1, 1, 86, 86, 39, 21, 22},     // 256-271 CPUs
      {43, 1, 1, 92, 92, 41, 22, 23},     // 272-287 CPUs
      {44, 1, 1, 98, 98, 43, 23, 24},     // 288-303 CPUs
      {45, 1, 1, 104, 104, 45, 24, 25},   // 304-319 CPUs
      {46, 1, 1, 110, 110, 47, 25, 26},   // 320-335 CPUs
      {47, 1, 1, 116, 116, 49, 26, 27},   // 336-351 CPUs
      {48, 1, 1, 122, 122, 51, 27, 28},   // 352-367 CPUs
      {49, 1, 1, 128, 128, 53, 28, 29},   // 368-383 CPUs
      {50, 1, 1, 134, 134, 55, 29, 30},   // 384-399 CPUs
      {51, 1, 1, 140, 140, 57, 30, 31},   // 400-415 CPUs
      {52, 1, 1, 146, 146, 59, 31, 32},   // 416-431 CPUs
      {53, 1, 1, 152, 152, 61, 32, 33},   // 432-447 CPUs
      {54, 1, 1, 158, 158, 63, 33, 34},   // 448-463 CPUs
      {55, 1, 1, 164, 164, 65, 34, 35},   // 464-479 CPUs
      {56, 1, 1, 170, 170, 67, 35, 36},   // 480-495 CPUs
      {57, 1, 1, 176, 176, 69, 36, 37},   // 496-511 CPUs
      {58, 1, 1, 182, 182, 71, 37, 38},   // 512-527 CPUs
      {59, 1, 1, 188, 188, 73, 38, 39},   // 528-543 CPUs
      {60, 1, 1, 194, 194, 75, 39, 40},   // 544-559 CPUs
      {61, 1, 1, 200, 200, 77, 40, 41},   // 560-575 CPUs
      {62, 1, 1, 206, 206, 79, 41, 42},   // 576-591 CPUs
      {63, 1, 1, 212, 212, 81, 42, 43},   // 592-607 CPUs
      {64, 1, 1, 218, 218, 83, 43, 44},   // 608-623 CPUs
      {65, 1, 1, 224, 224, 85, 44, 45},   // 624-639 CPUs
      {66, 1, 1, 230, 230, 87, 45, 46},   // 640-655 CPUs
      {67, 1, 1, 236, 236, 89, 46, 47},   // 656-671 CPUs
      {68, 1, 1, 242, 242, 91, 47, 48},   // 672-687 CPUs
      {69, 1, 1, 248, 248, 93, 48, 49},   // 688-703 CPUs
      {70, 1, 1, 254, 254, 95, 49, 50},   // 704-719 CPUs
      {71, 1, 1, 260, 260, 97, 50, 51},   // 720-735 CPUs
      {72, 1, 1, 266, 266, 99, 51, 52},   // 736-751 CPUs
      {73, 1, 1, 272, 272, 101, 52, 53},  // 752-767 CPUs
      {74, 1, 1, 278, 278, 103, 53, 54},  // 768-783 CPUs
      {75, 1, 1, 284, 284, 105, 54, 55},  // 784-799 CPUs
      {76, 1, 1, 290, 290, 107, 55, 56},  // 800-815 CPUs
      {77, 1, 1, 296, 296, 109, 56, 57},  // 816-831 CPUs
      {78, 1, 1, 302, 302, 111, 57, 58},  // 832-847 CPUs
      {79, 1, 1, 308, 308, 113, 58, 59},  // 848-863 CPUs
      {80, 1, 1, 314, 314, 115, 59, 60},  // 864-879 CPUs
      {81, 1, 1, 320, 320, 117, 60, 61},  // 880-895 CPUs
      {82, 1, 1, 326, 326, 119, 61, 62},  // 896-911 CPUs
      {83, 1, 1, 332, 332, 121, 62, 63},  // 912-927 CPUs
  };
  Uint32 cpu_cnt;
  Uint32 num_cpus_per_core;
  if (num_cpus == 0) {
    struct ndb_hwinfo *hwinfo = Ndb_GetHWInfo(false);
    cpu_cnt = hwinfo->cpu_cnt;
    if (cpu_cnt == 0) {
      cpu_cnt = hwinfo->cpu_cnt_max;
      cpu_cnt = MIN(cpu_cnt, MAX_USED_NUM_CPUS);
    }
#if 0
    /* Consistency check of above tables */
    Uint32 expected_map_id = 0;
    Uint32 save_cpu_cnt = 1;
    for (Uint32 i = 0; i < 255; i++)
    {
      Uint32 cpu_cnt = map_table[i].cpu_cnt;
      require(cpu_cnt == (i + 1));
      Uint32 map_id = map_table[i].mapped_id;
      if (map_id != expected_map_id)
      {
        require(map_id == (expected_map_id + 1));
        expected_map_id = map_id;
        save_cpu_cnt = cpu_cnt;
      }
      Uint32 main_threads = table[map_id].main_threads;
      Uint32 rep_threads = table[map_id].rep_threads;
      Uint32 ldm_threads = table[map_id].ldm_threads;
      Uint32 query_threads = table[map_id].query_threads;
      Uint32 tc_threads = table[map_id].tc_threads;
      Uint32 send_threads = table[map_id].send_threads;
      Uint32 recv_threads = table[map_id].recv_threads;
      Uint32 tot_threads = main_threads +
                           rep_threads +
                           ldm_threads +
                           query_threads +
                           tc_threads +
                           send_threads +
                           recv_threads;
      require(tot_threads == save_cpu_cnt);
    }
#endif
    /**
     * We make use of all CPUs, but we avoid using about 10% of the
     * CPUs in the machine. The idea with this scheme is to ensure
     * that we have sufficient CPU resources to handle interrupts,
     * OS kernel execution and the IO threads, connection threads
     * and other support threads.
     *
     * We divide by 10 to derive this number, this means that with
     * small number of CPUs we will use all CPUs. This is ok since
     * the main thread, rep thread and some other thread is usually
     * not fully occupied in those configurations.
     *
     * One special area to consider is the use of compressed LCPs,
     * compressed backups and encrypted file systems. This will
     * require a heavy CPU burden on the IO threads. This load could
     * be substantial. In this case we save 25% of the CPUs instead.
     */
    /**
     * The number of recover threads should be such that we have one
     * recover thread per CPU. However the LDM threads is also used
     * as recover thread, thus we can decrease the amount of recover
     * threads with the amount of LDM threads.
     */
    num_cpus_per_core = hwinfo->num_cpu_per_core;
    if (cpu_cnt >= 16 && cpu_cnt == hwinfo->cpu_cnt_max &&
        num_cpus_per_core > 2) {
      if (num_cpus_per_core == 3) {
        cpu_cnt *= 2;
        cpu_cnt /= 3;
        g_num_query_threads_per_ldm = 2;
      } else {
        num_cpus_per_core = 4;
        cpu_cnt /= 2;
        g_num_query_threads_per_ldm = 3;
      }
    } else {
      num_cpus_per_core = 2;
      g_num_query_threads_per_ldm = 1;
    }
  } else {
    cpu_cnt = num_cpus;
    num_cpus_per_core = 2;
  }

  Uint32 overhead_cpu_cnt = cpu_cnt / 10;
  Uint32 used_cpu_cnt = cpu_cnt - overhead_cpu_cnt;

  require(used_cpu_cnt > 0);
  Uint32 used_map_id;
  if (used_cpu_cnt >= 256) {
    used_map_id = 42;
    Uint32 extra_map_id = (used_cpu_cnt - 256) / 16;
    used_map_id += extra_map_id;
    used_map_id = MIN(used_map_id, 83);
  } else {
    used_map_id = map_table[used_cpu_cnt - 1].mapped_id;
  }
  main_threads = table[used_map_id].main_threads;
  rep_threads = table[used_map_id].rep_threads;
  ldm_threads = table[used_map_id].ldm_threads;
  query_threads = table[used_map_id].query_threads;
  tc_threads = table[used_map_id].tc_threads;
  send_threads = table[used_map_id].send_threads;
  recv_threads = table[used_map_id].recv_threads;

  recover_threads = cpu_cnt - (ldm_threads + query_threads);

  if (ldm_threads == 0) {
    // One worker instance of ldm blocks run in main or recv thread
    require(recover_threads > 0);
    recover_threads--;
  }

  Uint32 tot_threads = main_threads;
  tot_threads += rep_threads;
  tot_threads += ldm_threads;
  tot_threads += query_threads;
  tot_threads += tc_threads;
  tot_threads += recv_threads;

  if (tot_threads > NDBMT_MAX_BLOCK_INSTANCES) {
    /* Have to ensure total number of block instances are not beyond limit */
    recover_threads -= (tot_threads - NDBMT_MAX_BLOCK_INSTANCES);
  }
  /**
   *
   * Ignore this calculation for now
  if (num_cpus_per_core == 3)
  {
    query_threads *= 2;

    tc_threads *= 3;
    tc_threads /= 2;
    send_threads *= 3;
    send_threads /= 2;
    recv_threads *= 3;
    recv_threads /= 2;
    recover_threads *= 3;
    recover_threads /= 2;
  }
  else if (num_cpus_per_core == 4)
  {
    query_threads *= 3;
    tc_threads *= 2;
    send_threads *= 2;
    recv_threads *= 2;
    recover_threads *= 2;
  }
  */
}

int THRConfig::do_parse(unsigned realtime, unsigned spintime, unsigned num_cpus,
                        unsigned &num_rr_groups) {
  Uint32 tc_threads = 0;
  Uint32 ldm_threads = 0;
  Uint32 query_threads = 0;
  Uint32 recover_threads = 0;
  Uint32 main_threads = 0;
  Uint32 rep_threads = 0;
  Uint32 send_threads = 0;
  Uint32 recv_threads = 0;
  compute_automatic_thread_config(num_cpus, tc_threads, ldm_threads,
                                  query_threads, recover_threads, main_threads,
                                  rep_threads, send_threads, recv_threads);
  DEB_AUTO_THREAD_CONFIG(
      ("Auto thread config uses:"
       " %u LDM threads, "
       "%u Query threads, "
       "%u Recover threads, "
       "%u main threads, "
       "%u rep threads, "
       "%u recv threads, "
       "%u send threads",
       ldm_threads, query_threads, recover_threads, main_threads, rep_threads,
       recv_threads, send_threads));
  for (Uint32 i = 0; i < main_threads; i++) {
    add(T_MAIN, realtime, spintime);
  }
  for (Uint32 i = 0; i < rep_threads; i++) {
    add(T_REP, realtime, spintime);
  }
  /**
   * We add an IO thread to handle the IO threads.
   * In automatic thread configuration we do not bind these threads
   * to any particular CPUs.
   *
   * We add an index build thread to ensure that index builds can be
   * parallelised although not specifically configured to do so.
   * In automatic thread configuration the index build threads are
   * not bound to any specific CPU.
   *
   * If the user need to configure those for CPU locking the user
   * can still use ThreadConfig, the automated will make one choice
   * of automated thread config that is based on the CPUs that the
   * OS has bound the ndbmtd process to.
   */
  add(T_IO, realtime, 0);
  add(T_IXBLD, realtime, 0);
  add(T_WD, realtime, 0);
  for (Uint32 i = 0; i < ldm_threads; i++) {
    add(T_LDM, realtime, spintime);
  }
  for (Uint32 i = 0; i < tc_threads; i++) {
    add(T_TC, realtime, spintime);
  }
  for (Uint32 i = 0; i < query_threads; i++) {
    add(T_QUERY, realtime, spintime);
  }
  if (recover_threads > query_threads) {
    Uint32 num_recover_threads_only = recover_threads - query_threads;
    for (Uint32 i = 0; i < num_recover_threads_only; i++) {
      add(T_RECOVER, realtime, spintime);
    }
  }
  for (Uint32 i = 0; i < send_threads; i++) {
    add(T_SEND, realtime, spintime);
  }
  for (Uint32 i = 0; i < recv_threads; i++) {
    add(T_RECV, realtime, spintime);
  }
  struct ndb_hwinfo *hwinfo = Ndb_GetHWInfo(false);
  if (hwinfo->is_cpuinfo_available && num_cpus == 0) {
    /**
     * With CPU information available we will perform CPU locking as well
     * in an automated fashion. We have prepared the HW information such
     * that we can simply assign the CPUs from the CPU map.
     */
    Ndb_SetOnlineAsVirtL3CPU();
    Uint32 num_query_threads_per_ldm = g_num_query_threads_per_ldm;
    num_rr_groups = Ndb_CreateCPUMap(ldm_threads, num_query_threads_per_ldm);
    DEB_AUTO_THREAD_CONFIG(("Number of RR Groups = %u", num_rr_groups));
    Uint32 next_cpu_id = Ndb_GetFirstCPUInMap();
    Uint32 query_instance = 0;
    for (Uint32 i = 0; i < ldm_threads; i++) {
      require(next_cpu_id != Uint32(RNIL));
      m_threads[T_LDM][i].m_bind_no = next_cpu_id;
      m_threads[T_LDM][i].m_bind_type = T_Thread::B_CPU_BIND;
      next_cpu_id = Ndb_GetNextCPUInMap(next_cpu_id);
      m_threads[T_LDM][i].m_core_bind = true;
      for (Uint32 j = 0; j < num_query_threads_per_ldm; j++) {
        require(next_cpu_id != Uint32(RNIL));
        m_threads[T_QUERY][query_instance].m_bind_no = next_cpu_id;
        m_threads[T_QUERY][query_instance].m_bind_type = T_Thread::B_CPU_BIND;
        m_threads[T_QUERY][query_instance].m_core_bind = true;
        next_cpu_id = Ndb_GetNextCPUInMap(next_cpu_id);
        query_instance++;
      }
    }
    for (Uint32 i = 0; i < tc_threads; i++) {
      require(next_cpu_id != Uint32(RNIL));
      m_threads[T_TC][i].m_bind_no = next_cpu_id;
      m_threads[T_TC][i].m_bind_type = T_Thread::B_CPU_BIND;
      m_threads[T_TC][i].m_core_bind = true;
      next_cpu_id = Ndb_GetNextCPUInMap(next_cpu_id);
    }
    for (Uint32 i = 0; i < send_threads; i++) {
      require(next_cpu_id != Uint32(RNIL));
      m_threads[T_SEND][i].m_bind_no = next_cpu_id;
      m_threads[T_SEND][i].m_bind_type = T_Thread::B_CPU_BIND;
      m_threads[T_SEND][i].m_core_bind = true;
      next_cpu_id = Ndb_GetNextCPUInMap(next_cpu_id);
    }
    for (Uint32 i = 0; i < recv_threads; i++) {
      require(next_cpu_id != Uint32(RNIL));
      m_threads[T_RECV][i].m_bind_no = next_cpu_id;
      m_threads[T_RECV][i].m_bind_type = T_Thread::B_CPU_BIND;
      m_threads[T_RECV][i].m_core_bind = true;
      next_cpu_id = Ndb_GetNextCPUInMap(next_cpu_id);
    }
    for (Uint32 i = 0; i < main_threads; i++) {
      require(next_cpu_id != Uint32(RNIL));
      m_threads[T_MAIN][i].m_bind_no = next_cpu_id;
      m_threads[T_MAIN][i].m_bind_type = T_Thread::B_CPU_BIND;
      m_threads[T_MAIN][i].m_core_bind = true;
      next_cpu_id = Ndb_GetNextCPUInMap(next_cpu_id);
    }
    for (Uint32 i = 0; i < rep_threads; i++) {
      require(next_cpu_id != Uint32(RNIL));
      m_threads[T_REP][i].m_bind_no = next_cpu_id;
      m_threads[T_REP][i].m_bind_type = T_Thread::B_CPU_BIND;
      m_threads[T_REP][i].m_core_bind = true;
      next_cpu_id = Ndb_GetNextCPUInMap(next_cpu_id);
    }
  } else {
    num_rr_groups = Ndb_GetRRGroups(ldm_threads);
  }
  return 0;
}

int THRConfig::do_parse(unsigned MaxNoOfExecutionThreads,
                        unsigned __ndbmt_lqh_threads, unsigned __ndbmt_classic,
                        unsigned realtime, unsigned spintime) {
  /**
   * This is old ndbd.cpp : get_multithreaded_config
   */
  if (__ndbmt_classic) {
    m_classic = true;
    add(T_LDM, realtime, spintime);
    add(T_MAIN, realtime, spintime);
    add(T_IO, realtime, 0);
    add(T_WD, realtime, 0);
    const bool allow_too_few_cpus = true;
    return do_bindings(allow_too_few_cpus);
  }

  Uint32 tcthreads = 0;
  Uint32 lqhthreads = 0;
  Uint32 sendthreads = 0;
  Uint32 recvthreads = 1;
  switch (MaxNoOfExecutionThreads) {
    case 0:
    case 1:
    case 2:
    case 3:
      lqhthreads = 1;  // TC + receiver + SUMA + LQH
      break;
    case 4:
    case 5:
    case 6:
      lqhthreads = 2;  // TC + receiver + SUMA + 2 * LQH
      break;
    case 7:
    case 8:
      lqhthreads = 4;  // TC + receiver + SUMA + 4 * LQH
      break;
    default:
      computeThreadConfig(MaxNoOfExecutionThreads, tcthreads, lqhthreads,
                          sendthreads, recvthreads);
  }

  if (__ndbmt_lqh_threads) {
    lqhthreads = __ndbmt_lqh_threads;
  }

  add(T_MAIN, realtime, spintime); /* Global */
  add(T_REP, realtime, spintime);  /* Local, main consumer is SUMA */
  for (Uint32 i = 0; i < recvthreads; i++) {
    add(T_RECV, realtime, spintime);
  }
  add(T_IO, realtime, 0);
  add(T_WD, realtime, 0);
  for (Uint32 i = 0; i < lqhthreads; i++) {
    add(T_LDM, realtime, spintime);
  }
  for (Uint32 i = 0; i < tcthreads; i++) {
    add(T_TC, realtime, spintime);
  }
  for (Uint32 i = 0; i < sendthreads; i++) {
    add(T_SEND, realtime, spintime);
  }

  // If we have set TC-threads...we say that this is "new" code
  // and give error for having too few CPU's in mask compared to #threads
  // started
  const bool allow_too_few_cpus =
      (tcthreads == 0 && sendthreads == 0 && recvthreads == 1);
  int res = do_bindings(allow_too_few_cpus);
  if (res != 0) {
    return res;
  }
  return do_validate();
}

void THRConfig::lock_io_threads() {
  /**
   * Use LockIoThreadsToCPU also to lock to Watchdog, SocketServer
   * and SocketClient for backwards compatibility reasons,
   * preferred manner is to only use ThreadConfig
   */
  if (m_LockIoThreadsToCPU.count() == 1) {
    m_threads[T_IO][0].m_bind_type = T_Thread::B_CPU_BIND;
    m_threads[T_IO][0].m_bind_no = m_LockIoThreadsToCPU.getBitNo(0);
    m_threads[T_WD][0].m_bind_type = T_Thread::B_CPU_BIND;
    m_threads[T_WD][0].m_bind_no = m_LockIoThreadsToCPU.getBitNo(0);
  } else if (m_LockIoThreadsToCPU.count() > 1) {
    unsigned no = createCpuSet(m_LockIoThreadsToCPU, true);
    m_threads[T_IO][0].m_bind_type = T_Thread::B_CPUSET_BIND;
    m_threads[T_IO][0].m_bind_no = no;
    m_threads[T_WD][0].m_bind_type = T_Thread::B_CPUSET_BIND;
    m_threads[T_WD][0].m_bind_no = no;
  }
}

int THRConfig::do_bindings(bool allow_too_few_cpus) {
  /* Track all cpus that we lock threads to */
  SparseBitmask allCpus;
  allCpus.bitOR(m_LockIoThreadsToCPU);
  lock_io_threads();
  /**
   * Check that no permanent cpu_sets overlap
   */
  for (unsigned i = 0; i < m_perm_cpu_sets.size(); i++) {
    const SparseBitmask &a = m_cpu_sets[m_perm_cpu_sets[i]];
    allCpus.bitOR(a);

    for (unsigned j = i + 1; j < m_perm_cpu_sets.size(); j++) {
      const SparseBitmask &b = m_cpu_sets[m_perm_cpu_sets[j]];
      if (a.overlaps(b)) {
        m_err_msg.assfmt("Overlapping cpuset's [ %s ] and [ %s ]",
                         a.str().c_str(), b.str().c_str());
        return -1;
      }
    }
  }

  /**
   * Check that no permanent cpu_sets overlap with cpu_bound
   */
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++) {
    for (unsigned j = 0; j < m_threads[i].size(); j++) {
      if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BIND) {
        unsigned cpu = m_threads[i][j].m_bind_no;
        allCpus.set(cpu);
        for (unsigned k = 0; k < m_perm_cpu_sets.size(); k++) {
          const SparseBitmask &cpuSet = m_cpu_sets[m_perm_cpu_sets[k]];
          if (cpuSet.get(cpu)) {
            m_err_msg.assfmt("Overlapping cpubind %u with cpuset [ %s ]", cpu,
                             cpuSet.str().c_str());

            return -1;
          }
        }
      }
    }
  }

  /**
   * Remove all already bound threads from LockExecuteThreadToCPU-mask
   */
  for (unsigned i = 0; i < m_perm_cpu_sets.size(); i++) {
    const SparseBitmask &cpuSet = m_cpu_sets[m_perm_cpu_sets[i]];
    for (unsigned j = 0; j < cpuSet.count(); j++) {
      m_LockExecuteThreadToCPU.clear(cpuSet.getBitNo(j));
    }
  }

  unsigned cnt_unbound = 0;
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++) {
    if (!m_entries[i].m_is_exec_thd) {
      /* Only interested in execution threads here */
      continue;
    }
    for (unsigned j = 0; j < m_threads[i].size(); j++) {
      if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BIND) {
        unsigned cpu = m_threads[i][j].m_bind_no;
        m_LockExecuteThreadToCPU.clear(cpu);
      } else if (m_threads[i][j].m_bind_type == T_Thread::B_UNBOUND) {
        cnt_unbound++;
      }
    }
  }

  if (m_LockExecuteThreadToCPU.count()) {
    /**
     * This is old mt.cpp : setcpuaffinity
     */
    SparseBitmask &mask = m_LockExecuteThreadToCPU;
    unsigned cnt = mask.count();
    unsigned num_threads = cnt_unbound;
    bool isMtLqh = !m_classic;

    allCpus.bitOR(m_LockExecuteThreadToCPU);
    if (cnt < num_threads) {
      m_info_msg.assfmt(
          "WARNING: Too few CPU's specified with "
          "LockExecuteThreadToCPU. Only %u specified "
          " but %u was needed, this may cause contention.\n",
          cnt, num_threads);

      if (!allow_too_few_cpus) {
        m_err_msg.assfmt(
            "Too few CPU's specifed with LockExecuteThreadToCPU. "
            "This is not supported when using multiple TC threads");
        return -1;
      }
    }

    if (cnt >= num_threads) {
      m_info_msg.appfmt("Assigning each thread its own CPU\n");
      unsigned no = 0;
      for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++) {
        if (!m_entries[i].m_is_exec_thd) continue;
        for (unsigned j = 0; j < m_threads[i].size(); j++) {
          if (m_threads[i][j].m_bind_type == T_Thread::B_UNBOUND) {
            m_threads[i][j].m_bind_type = T_Thread::B_CPU_BIND;
            m_threads[i][j].m_bind_no = mask.getBitNo(no);
            no++;
          }
        }
      }
    } else if (cnt == 1) {
      unsigned cpu = mask.getBitNo(0);
      m_info_msg.appfmt("Assigning all threads to CPU %u\n", cpu);
      for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++) {
        if (!m_entries[i].m_is_exec_thd) continue;
        bind_unbound(m_threads[i], cpu);
      }
    } else if (isMtLqh) {
      unsigned unbound_ldm = count_unbound(m_threads[T_LDM]);
      if (cnt > unbound_ldm) {
        /**
         * let each LQH have it's own CPU and rest share...
         */
        m_info_msg.append(
            "Assigning LQH threads to dedicated CPU(s) and "
            "other threads will share remaining\n");
        unsigned cpu = mask.find(0);
        for (unsigned i = 0; i < m_threads[T_LDM].size(); i++) {
          if (m_threads[T_LDM][i].m_bind_type == T_Thread::B_UNBOUND) {
            m_threads[T_LDM][i].m_bind_type = T_Thread::B_CPU_BIND;
            m_threads[T_LDM][i].m_bind_no = cpu;
            mask.clear(cpu);
            cpu = mask.find(cpu + 1);
          }
        }

        cpu = mask.find(0);
        Uint32 num_main_threads =
            getThreadCount(T_REP) + getThreadCount(T_MAIN);
        if (num_main_threads == 2) {
          bind_unbound(m_threads[T_MAIN], cpu);
          bind_unbound(m_threads[T_REP], cpu);
        } else {
          bind_unbound(m_threads[T_MAIN], cpu);
        }
        if ((cpu = mask.find(cpu + 1)) == mask.NotFound) {
          cpu = mask.find(0);
        }
        bind_unbound(m_threads[T_RECV], cpu);
      } else {
        // put receiver, tc, backup/suma in 1 thread,
        // and round robin LQH for rest
        unsigned cpu = mask.find(0);
        m_info_msg.appfmt(
            "Assigning LQH threads round robin to CPU(s) and "
            "other threads will share CPU %u\n",
            cpu);
        Uint32 num_main_threads =
            getThreadCount(T_REP) + getThreadCount(T_MAIN);
        if (num_main_threads == 2) {
          bind_unbound(m_threads[T_MAIN], cpu);
          bind_unbound(m_threads[T_REP], cpu);
        } else {
          bind_unbound(m_threads[T_MAIN], cpu);
        }
        bind_unbound(m_threads[T_RECV], cpu);
        mask.clear(cpu);

        cpu = mask.find(0);
        for (unsigned i = 0; i < m_threads[T_LDM].size(); i++) {
          if (m_threads[T_LDM][i].m_bind_type == T_Thread::B_UNBOUND) {
            m_threads[T_LDM][i].m_bind_type = T_Thread::B_CPU_BIND;
            m_threads[T_LDM][i].m_bind_no = cpu;
            if ((cpu = mask.find(cpu + 1)) == mask.NotFound) {
              cpu = mask.find(0);
            }
          }
        }
      }
    } else {
      unsigned cpu = mask.find(0);
      m_info_msg.appfmt(
          "Assigning LQH thread to CPU %u and "
          "other threads will share\n",
          cpu);
      bind_unbound(m_threads[T_LDM], cpu);
      cpu = mask.find(cpu + 1);
      bind_unbound(m_threads[T_MAIN], cpu);
      bind_unbound(m_threads[T_RECV], cpu);
    }
  }
  if (m_threads[T_IXBLD].size() == 0) {
    /**
     * No specific IDXBLD configuration from the user
     * In this case : IDXBLD should be :
     *  - Unbound if IO is unbound - use any core
     *  - Bound to the full set of bound threads if
     *    IO is bound - assumes nothing better for
     *    threads to do.
     */
    const T_Thread *io_thread = &m_threads[T_IO][0];
    add(T_IXBLD, 0, 0);

    if (io_thread->m_bind_type != T_Thread::B_UNBOUND) {
      /* IO thread is bound, we should be bound to
       * all defined threads
       */
      BaseString allCpusString = allCpus.str();
      m_info_msg.appfmt(
          "IO threads explicitly bound, "
          "but IDXBLD threads not.  "
          "Binding IDXBLD to %s.\n",
          allCpusString.c_str());

      unsigned bind_no = createCpuSet(allCpus, false);

      m_threads[T_IXBLD][0].m_bind_type = T_Thread::B_CPUSET_BIND;
      m_threads[T_IXBLD][0].m_bind_no = bind_no;
    }
  }
  return 0;
}

unsigned THRConfig::count_unbound(const Vector<T_Thread> &vec) const {
  unsigned cnt = 0;
  for (unsigned i = 0; i < vec.size(); i++) {
    if (vec[i].m_bind_type == T_Thread::B_UNBOUND) cnt++;
  }
  return cnt;
}

void THRConfig::bind_unbound(Vector<T_Thread> &vec, unsigned cpu) {
  for (unsigned i = 0; i < vec.size(); i++) {
    if (vec[i].m_bind_type == T_Thread::B_UNBOUND) {
      vec[i].m_bind_type = T_Thread::B_CPU_BIND;
      vec[i].m_bind_no = cpu;
    }
  }
}

int THRConfig::do_validate() {
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++) {
    /**
     * Check that there aren't too many or too few of any thread type
     */
    if (m_threads[i].size() > getMaxEntries(i)) {
      m_err_msg.assfmt("Too many instances(%u) of %s max supported: %u",
                       m_threads[i].size(), getEntryName(i), getMaxEntries(i));
      return -1;
    }
    if (m_threads[i].size() < getMinEntries(i)) {
      m_err_msg.assfmt("Too few instances(%u) of %s min supported: %u",
                       m_threads[i].size(), getEntryName(i), getMinEntries(i));
      return -1;
    }
  }

  if (m_threads[T_REP].size() > 0 && m_threads[T_MAIN].size() == 0) {
    m_err_msg.assfmt("Can't set a %s thread without a %s thread.",
                     getEntryName(T_REP), getEntryName(T_MAIN));
    return -1;
  }
  return 0;
}

void THRConfig::append_name(const char *name, const char *sep,
                            bool &append_name_flag) {
  if (!append_name_flag) {
    m_cfg_string.append(sep);
    m_cfg_string.append(name);
    append_name_flag = true;
  }
}

const char *THRConfig::getConfigString() {
  m_cfg_string.clear();
  const char *sep = "";
  const char *end_sep;
  const char *start_sep;
  const char *between_sep;
  bool append_name_flag;
  if (getThreadCount() == 0) {
    return m_cfg_string.c_str();
  }
  for (unsigned i = 0; i < NDB_ARRAY_SIZE(m_threads); i++) {
    const char *name = getEntryName(i);
    if (m_threads[i].size() == 0 && m_setInThreadConfig.get(i)) {
      sep = m_cfg_string.empty() ? "" : ",";
      m_cfg_string.appfmt("%s%s={count=0}", sep, name);
      sep = ",";
    }
    if (m_threads[i].size()) {
      for (unsigned j = 0; j < m_threads[i].size(); j++) {
        start_sep = "={";
        end_sep = "";
        between_sep = "";
        append_name_flag = false;
        if (m_entries[i].m_is_exec_thd) {
          append_name(name, sep, append_name_flag);
          sep = ",";
        }
        if (m_threads[i][j].m_bind_type != T_Thread::B_UNBOUND) {
          append_name(name, sep, append_name_flag);
          sep = ",";
          m_cfg_string.append(start_sep);
          end_sep = "}";
          start_sep = "";
          if (m_threads[i][j].m_bind_type == T_Thread::B_CPU_BIND) {
            m_cfg_string.appfmt("cpubind=%u", m_threads[i][j].m_bind_no);
            between_sep = ",";
          } else if (m_threads[i][j].m_bind_type ==
                     T_Thread::B_CPU_BIND_EXCLUSIVE) {
            m_cfg_string.appfmt("cpubind_exclusive=%u",
                                m_threads[i][j].m_bind_no);
            between_sep = ",";
          } else if (m_threads[i][j].m_bind_type == T_Thread::B_CPUSET_BIND) {
            m_cfg_string.appfmt(
                "cpuset=%s",
                m_cpu_sets[m_threads[i][j].m_bind_no].str().c_str());
            between_sep = ",";
          } else if (m_threads[i][j].m_bind_type ==
                     T_Thread::B_CPUSET_EXCLUSIVE_BIND) {
            m_cfg_string.appfmt(
                "cpuset_exclusive=%s",
                m_cpu_sets[m_threads[i][j].m_bind_no].str().c_str());
            between_sep = ",";
          }
        }
        if (m_threads[i][j].m_spintime || m_threads[i][j].m_realtime) {
          append_name(name, sep, append_name_flag);
          sep = ",";
          m_cfg_string.append(start_sep);
          end_sep = "}";
          if (m_threads[i][j].m_spintime) {
            m_cfg_string.append(between_sep);
            m_cfg_string.appfmt("spintime=%u", m_threads[i][j].m_spintime);
            between_sep = ",";
          }
          if (m_threads[i][j].m_realtime) {
            m_cfg_string.append(between_sep);
            m_cfg_string.appfmt("realtime=%u", m_threads[i][j].m_realtime);
            between_sep = ",";
          }
        }
        m_cfg_string.append(end_sep);
      }
    }
  }
  return m_cfg_string.c_str();
}

Uint32 THRConfig::getThreadCount() const {
  // Note! not counting T_IO
  Uint32 cnt = 0;
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_threads); i++) {
    if (m_entries[i].m_is_exec_thd) {
      cnt += m_threads[i].size();
    }
  }
  return cnt;
}

Uint32 THRConfig::getThreadCount(T_Type type) const {
  for (Uint32 i = 0; i < NDB_ARRAY_SIZE(m_threads); i++) {
    if (i == (Uint32)type) {
      return m_threads[i].size();
    }
  }
  return 0;
}

const char *THRConfig::getErrorMessage() const {
  if (m_err_msg.empty()) return nullptr;
  return m_err_msg.c_str();
}

const char *THRConfig::getInfoMessage() const {
  if (m_info_msg.empty()) return nullptr;
  return m_info_msg.c_str();
}

int THRConfig::handle_spec(const char *str, unsigned realtime,
                           unsigned spintime) {
  ParseThreadConfiguration parser(str, &m_parse_entries[0],
                                  NDB_ARRAY_SIZE(m_parse_entries), &m_params[0],
                                  NDB_ARRAY_SIZE(m_params), m_err_msg);

  do {
    unsigned int loc_type;
    int ret_code;
    ParamValue values[NDB_ARRAY_SIZE(m_params)];
    values[IX_COUNT].unsigned_val = 1;
    values[IX_REALTIME].unsigned_val = realtime;
    values[IX_THREAD_PRIO].unsigned_val = NO_THREAD_PRIO_USED;
    values[IX_SPINTIME].unsigned_val = spintime;

    if (parser.read_params(values, NDB_ARRAY_SIZE(m_params), &loc_type,
                           &ret_code, true) != 0) {
      /* Parser is done either successful or not */
      return ret_code;
    }

    T_Type type = (T_Type)loc_type;
    m_setInThreadConfig.set(loc_type);

    int cpu_values = 0;
    if (values[IX_CPUBIND].found) cpu_values++;
    if (values[IX_CPUBIND_EXCLUSIVE].found) cpu_values++;
    if (values[IX_CPUSET].found) cpu_values++;
    if (values[IX_CPUSET_EXCLUSIVE].found) cpu_values++;
    if (cpu_values > 1) {
      m_err_msg.assfmt(
          "Only one of cpubind, cpuset and cpuset_exclusive"
          " can be specified");
      return -1;
    }
    if (values[IX_REALTIME].found && values[IX_THREAD_PRIO].found &&
        values[IX_REALTIME].unsigned_val != 0) {
      m_err_msg.assfmt(
          "Only one of realtime and thread_prio can be used to"
          " change thread priority in the OS scheduling");
      return -1;
    }
    if (values[IX_THREAD_PRIO].found &&
        values[IX_THREAD_PRIO].unsigned_val > MAX_THREAD_PRIO_NUMBER) {
      m_err_msg.assfmt(
          "thread_prio must be between 0 and 10, where 10 is the"
          " highest priority");
      return -1;
    }
    if (values[IX_SPINTIME].found && !m_entries[type].m_is_exec_thd) {
      m_err_msg.assfmt("Cannot set spintime on non-exec threads");
      return -1;
    }
    if (values[IX_NOSEND].found &&
        !(type == T_LDM || type == T_TC || type == T_MAIN || type == T_REP)) {
      m_err_msg.assfmt("Can only set nosend on main, ldm, tc and rep threads");
      return -1;
    }
    if (values[IX_THREAD_PRIO].found && type == T_IXBLD) {
      m_err_msg.assfmt("Cannot set threadprio on idxbld threads");
      return -1;
    }
    if (values[IX_REALTIME].found && type == T_IXBLD) {
      m_err_msg.assfmt("Cannot set realtime on idxbld threads");
      return -1;
    }

    unsigned cnt = values[IX_COUNT].unsigned_val;
    const int index = m_threads[type].size();
    for (unsigned i = 0; i < cnt; i++) {
      add(type, values[IX_REALTIME].unsigned_val,
          values[IX_SPINTIME].unsigned_val);
    }

    assert(m_threads[type].size() == index + cnt);
    if (values[IX_CPUSET].found) {
      const SparseBitmask &mask = values[IX_CPUSET].mask_val;
      unsigned no = createCpuSet(mask, m_entries[type].m_is_permanent);
      for (unsigned i = 0; i < cnt; i++) {
        m_threads[type][index + i].m_bind_type = T_Thread::B_CPUSET_BIND;
        m_threads[type][index + i].m_bind_no = no;
      }
    } else if (values[IX_CPUSET_EXCLUSIVE].found) {
      const SparseBitmask &mask = values[IX_CPUSET_EXCLUSIVE].mask_val;
      unsigned no = createCpuSet(mask, m_entries[type].m_is_permanent);
      for (unsigned i = 0; i < cnt; i++) {
        m_threads[type][index + i].m_bind_type =
            T_Thread::B_CPUSET_EXCLUSIVE_BIND;
        m_threads[type][index + i].m_bind_no = no;
      }
    } else if (values[IX_CPUBIND].found) {
      const SparseBitmask &mask = values[IX_CPUBIND].mask_val;
      if (mask.count() < cnt) {
        m_err_msg.assfmt("%s: trying to bind %u threads to %u cpus [%s]",
                         getEntryName(type), cnt, mask.count(),
                         mask.str().c_str());
        return -1;
      }
      for (unsigned i = 0; i < cnt; i++) {
        m_threads[type][index + i].m_bind_type = T_Thread::B_CPU_BIND;
        m_threads[type][index + i].m_bind_no = mask.getBitNo(i % mask.count());
      }
    } else if (values[IX_CPUBIND_EXCLUSIVE].found) {
      const SparseBitmask &mask = values[IX_CPUBIND_EXCLUSIVE].mask_val;
      if (mask.count() < cnt) {
        m_err_msg.assfmt("%s: trying to bind %u threads to %u cpus [%s]",
                         getEntryName(type), cnt, mask.count(),
                         mask.str().c_str());
        return -1;
      }
      for (unsigned i = 0; i < cnt; i++) {
        m_threads[type][index + i].m_bind_type = T_Thread::B_CPU_BIND_EXCLUSIVE;
        m_threads[type][index + i].m_bind_no = mask.getBitNo(i % mask.count());
      }
    }
    if (values[IX_THREAD_PRIO].found) {
      for (unsigned i = 0; i < cnt; i++) {
        m_threads[type][index + i].m_thread_prio =
            values[IX_THREAD_PRIO].unsigned_val;
      }
    }
    if (values[IX_NOSEND].found) {
      for (unsigned i = 0; i < cnt; i++) {
        m_threads[type][index + i].m_nosend = values[IX_NOSEND].unsigned_val;
      }
    }
  } while (1);
  return 0;
}

int THRConfig::do_validate_thread_counts() {
  for (Uint32 i = 0; i < T_END; i++) {
    /**
     * Checks that the thread count of each thread set in threadConfig
     * is >= m_min_cnt and  <= m_max_count
     */

    if (m_setInThreadConfig.get(i) &&
        m_threads[i].size() < m_entries[i].m_min_cnt) {
      m_err_msg.assfmt("Too few instances(%u) of %s min supported: %u",
                       m_threads[i].size(), getEntryName(i), getMinEntries(i));
      return -1;
    }
    if (m_setInThreadConfig.get(i) &&
        m_threads[i].size() > m_entries[i].m_max_cnt) {
      m_err_msg.assfmt("Too many instances(%u) of %s max supported: %u",
                       m_threads[i].size(), getEntryName(i), getMaxEntries(i));
      return -1;
    }
  }
  return 0;
}

int THRConfig::do_parse(const char *ThreadConfig, unsigned realtime,
                        unsigned spintime) {
  int ret = handle_spec(ThreadConfig, realtime, spintime);
  if (ret != 0) return ret;

  ret = do_validate_thread_counts();
  if (ret != 0) return ret;
  for (Uint32 i = 0; i < T_END; i++) {
    if (m_setInThreadConfig.get(i)) continue;
    while (m_threads[i].size() < m_entries[i].m_default_count) {
      add((T_Type)i, realtime, spintime);
    }
  }

  const bool allow_too_few_cpus = m_threads[T_TC].size() == 0 &&
                                  m_threads[T_SEND].size() == 0 &&
                                  m_threads[T_RECV].size() == 1;

  int res = do_bindings(allow_too_few_cpus);
  if (res != 0) {
    return res;
  }
  return do_validate();
}

unsigned THRConfig::createCpuSet(const SparseBitmask &mask, bool permanent) {
  /**
   * Create a cpuset according to the passed mask, and return its number
   * If one with that mask already exists, just return the existing
   * number.
   * A subset of all cpusets are on a 'permanent' list.  Permanent
   * cpusets must be non-overlapping.
   * Non permanent cpusets can overlap with permanent cpusets
   */
  unsigned i = 0;
  for (; i < m_cpu_sets.size(); i++) {
    if (m_cpu_sets[i].equal(mask)) {
      break;
    }
  }

  if (i == m_cpu_sets.size()) {
    /* Not already present */
    m_cpu_sets.push_back(mask);
  }
  if (permanent) {
    /**
     * Add to permanent cpusets list, if not already there
     * (existing cpuset could be !permanent)
     */
    unsigned j = 0;
    for (; j < m_perm_cpu_sets.size(); j++) {
      if (m_perm_cpu_sets[j] == i) {
        break;
      }
    }

    if (j == m_perm_cpu_sets.size()) {
      m_perm_cpu_sets.push_back(i);
    }
  }
  return i;
}

bool THRConfig::isThreadPermanent(T_Type type) {
  return m_entries[type].m_is_permanent;
}

template class Vector<SparseBitmask>;
template class Vector<THRConfig::T_Thread>;

#ifdef TEST_THR_CONFIG

#include <NdbTap.hpp>

TAPTEST(thr_config) {
  ndb_init();
  {
    THRConfig tmp;
    OK(tmp.do_parse(8, 0, 0, 0, 0) == 0);
  }

  /**
   * BASIC test
   */
  {
    const char *ok[] = {
        "main", "ldm", "recv", "rep", "main,rep,recv,ldm,ldm",
        "main,rep,recv,ldm={count=3},ldm",
        "main,rep,recv,ldm={cpubind=1-2,5,count=3},ldm",
        "main,rep,recv,ldm={ cpubind = 1- 2, 5 , count = 3 },ldm",
        "main,rep,recv,ldm={count=3,cpubind=1-2,5 },  ldm",
        "main,rep,recv,ldm={cpuset=1-3,count=3,realtime=0,spintime=0 },ldm",
        "main,rep,recv,ldm={cpuset=1-3,count=3,realtime=1,spintime=0 },ldm",
        "main,rep,recv,ldm={cpuset=1-3,count=3,realtime=0,spintime=1 },ldm",
        "main,rep,recv,ldm={cpuset=1-3,count=3,realtime=1,spintime=1 },ldm",
        "main,rep,recv,ldm,io={cpuset=3,4,6}",
        "main,rep,recv,ldm={cpuset_exclusive=1-3,count=3,realtime=1,spintime=1 "
        "},ldm",
        "main,rep,recv,ldm={cpubind_exclusive=1-3,count=3,realtime=1,spintime="
        "1 },ldm",
        "main,rep,recv,ldm={cpubind=1-3,count=3,thread_prio=10,spintime=1 "
        "},ldm",
        "main,rep,recv,ldm={},ldm", "main,rep,recv,ldm={},ldm,tc",
        "main,rep,recv,ldm={},ldm,tc,tc",
        /* Overlap idxbld + others */
        "main, rep, recv, ldm={count=4, cpuset=1-4}, tc={count=4, "
        "cpuset=5,6,7},"
        "io={cpubind=8}, idxbld={cpuset=1-8}",
        /* Overlap via cpubind */
        "main, rep, recv, ldm={count=1, cpubind=1}, idxbld={count=1, "
        "cpubind=1}",
        /* Overlap via same cpuset, with temp defined first */
        "main, rep, recv, idxbld={cpuset=1-4}, ldm={count=4, cpuset=1-4}",
        /* Io specified, no idxbuild, spreads over all 1-8 */
        "main, rep, recv, ldm={count=4, cpuset=1-4}, tc={count=4, "
        "cpuset=5,6,7},"
        "io={cpubind=8}",
        "main,rep,recv,ldm,ldm,ldm", /* 3 LDM's allowed */
        "main,ldm,recv,rep", /* Execution threads with default count > 0 only */
        "main,rep={count=0},recv,ldm",           /* 0 rep allowed */
        "main={count=0},rep={count=0},recv,ldm", /* 0 rep and 0 main allowed */
        "main={count=0},rep={count=0},recv,ldm={count=0}", /* 0 rep, 0 main and
                                                              0 ldm allowed */
        nullptr};

    const char *fail[] = {
        "ldm={cpubind=1,tc={cpubind=2}",     /* Missing } */
        "ldm,ldm,",                          /* Parse error, ending , */
        "ldm={count=4,}",                    /* No parameter after comma*/
        "ldm= {count = 3, }",                /* No parameter after comma*/
        "ldm , ldm , ",                      /* No parameter after comma*/
        "ldb,ldm",                           /* ldb non-existent thread type */
        "ldm={cpubind= 1 , cpuset=2 },ldm",  /* Cannot cpubind and cpuset */
        "ldm={count=4,cpubind=1-3},ldm",     /* 4 LDMs need 4 CPUs */
        "rep,recv,main,main,ldm,ldm",        /* More than 1 main */
        "recv,main,rep,rep,ldm,ldm",         /* More than 1 rep */
        "main={ keso=88, count=23},ldm,ldm", /* keso not allowed type */
        "recv,rep,idxbld={cpuset=1-4}, main={ cpuset=1-3 }, ldm={cpuset=3-4}",
        "main={ cpuset=1-3 }, ldm={cpubind=2}", /* Overlapping cpu sets */
        "rep, recv, main={ cpuset=1;3 }, ldm={cpubind=4}", /* ; not allowed
                                                              separator */
        "main={ cpuset=1,,3 }, ldm={cpubind=2}",           /* empty between , */
        "io={ spintime = 0 }",  /* Spintime on IO thread is not settable */
        "tc,tc,tc={count=161}", /* More than 160 TCs not allowed */
        "tc,tc,tc={count=3",    /* Missing } at end */
        "tc,tc,tc={count=3,count=3}",  /* count specified twice */
        "tc,tc,tc={count=3,cpuset;3}", /* ; instead of = */
        "tc,tc,tc={count=}",           /* Missing number */
        "tc,tc,tc={count=1234567890123456789012345}", /* Out of range */
        "tc,tc,tc={count=12345678901}",               /* Too large number */
        "tc,tc,tc={count=-1}",                        /* Negative number */
        "ldm={cpubind=1-3,count=3,realtime=1,spintime=1 , thread_prio = 10 "
        "},ldm",
        /* Cannot mix realtime and thread_prio */
        "ldm={cpubind=1-3,count=3,thread_prio=11,spintime=1 },ldm",
        "ldm={cpubind=1-3,count=3,thread_prio=-1,spintime=1 },ldm",
        /* thread_prio out of range */
        "idxbld={ spintime=12 }",
        "rep={count=1},main={count=0},recv,ldm", /* rep count=1 requires main
                                                    count=1 */
        "main={count=2}",                        /* too many main threads*/
        "recv={count=0}",                        /* too few recv threads*/
        nullptr};

    for (Uint32 i = 0; ok[i]; i++) {
      THRConfig tmp;
      int res = tmp.do_parse(ok[i], 0, 0);
      printf("do_parse(%s) => %s - %s\n", ok[i], res == 0 ? "OK" : "FAIL",
             res == 0 ? tmp.getConfigString() : tmp.getErrorMessage());
      OK(res == 0);
      {
        BaseString out(tmp.getConfigString());
        THRConfig check;
        OK(check.do_parse(out.c_str(), 0, 0) == 0);
        OK(strcmp(out.c_str(), check.getConfigString()) == 0);
      }
    }

    for (Uint32 i = 0; fail[i]; i++) {
      THRConfig tmp;
      int res = tmp.do_parse(fail[i], 0, 0);
      printf("do_parse(%s) => %s - %s\n", fail[i], res == 0 ? "OK" : "FAIL",
             res == 0 ? "" : tmp.getErrorMessage());
      OK(res != 0);
    }
  }
  {
    BaseString err_msg;
    static const struct ParseEntries loc_parse_entries[] = {// name type
                                                            {"string_type", 1}};
    static const struct ParseParams loc_params[] = {
        {"string", ParseParams::S_STRING},
        {"unsigned", ParseParams::S_UNSIGNED}};
    const char *ok[] =

        {"string_type={string=\"abc\"}", nullptr};
    const char *fail[] = {
        "string_type", /* Empty specification not allowed here */
        "string_type={string=\"01234567890123456789012345678901234\"}",
        /* String too long */
        nullptr};
    for (Uint32 i = 0; ok[i]; i++) {
      fprintf(stderr, "read_params: %s\n", ok[i]);
      ParamValue values[NDB_ARRAY_SIZE(loc_params)];
      ParseThreadConfiguration parser(
          ok[i], &loc_parse_entries[0], NDB_ARRAY_SIZE(loc_parse_entries),
          &loc_params[0], NDB_ARRAY_SIZE(loc_params), err_msg);
      int ret_code;
      unsigned int type;
      int ret = parser.read_params(values, NDB_ARRAY_SIZE(loc_params), &type,
                                   &ret_code, false);
      OK(ret_code == 0);
      OK(type == 1);
      OK(ret == 0);
    }
    for (Uint32 i = 0; fail[i]; i++) {
      fprintf(stderr, "read_params: %s\n", fail[i]);
      ParamValue values[NDB_ARRAY_SIZE(loc_params)];
      ParseThreadConfiguration parser(
          fail[i], &loc_parse_entries[0], NDB_ARRAY_SIZE(loc_parse_entries),
          &loc_params[0], NDB_ARRAY_SIZE(loc_params), err_msg);
      int ret_code;
      unsigned int type;
      int ret = parser.read_params(values, NDB_ARRAY_SIZE(loc_params), &type,
                                   &ret_code, false);
      OK(ret == 1);
      OK(ret_code == -1);
    }
  }

  {
    /**
     * Test interaction with LockExecuteThreadToCPU
     */
    const char *t[] = {
        /** threads, LockExecuteThreadToCPU, answer */
        "1-8", "main={count=0},rep={count=0},recv,ldm={count=4}", "OK",
        "main={count=0},ldm={cpubind=1},ldm={cpubind=2},ldm={cpubind=3},ldm={"
        "cpubind=4},recv={cpubind=5},rep={count=0}",

        "1-5", "main={count=0},rep={count=0},recv,ldm={count=4}", "OK",
        "main={count=0},ldm={cpubind=1},ldm={cpubind=2},ldm={cpubind=3},ldm={"
        "cpubind=4},recv={cpubind=5},rep={count=0}",

        "1-3", "main={count=0},rep={count=0},recv,ldm={count=4}", "OK",
        "main={count=0},ldm={cpubind=2},ldm={cpubind=3},ldm={cpubind=2},ldm={"
        "cpubind=3},recv={cpubind=1},rep={count=0}",

        "1-4", "main={count=0},rep={count=0},recv,ldm={count=4}", "OK",
        "main={count=0},ldm={cpubind=2},ldm={cpubind=3},ldm={cpubind=4},ldm={"
        "cpubind=2},recv={cpubind=1},rep={count=0}",

        "1-8", "main={count=0},rep={count=0},recv,ldm={count=4},io={cpubind=8}",
        "OK",
        "main={count=0},ldm={cpubind=1},ldm={cpubind=2},ldm={cpubind=3},ldm={"
        "cpubind=4},recv={cpubind=5},rep={count=0},io={cpubind=8},idxbld={"
        "cpuset=1,2,3,4,5,6,7,8}",

        "1-8",
        "main={count=0},rep={count=0},recv,ldm={count=4},io={cpubind=8},idxbld="
        "{cpuset=5,6,8}",
        "OK",
        "main={count=0},ldm={cpubind=1},ldm={cpubind=2},ldm={cpubind=3},ldm={"
        "cpubind=4},recv={cpubind=5},rep={count=0},io={cpubind=8},idxbld={"
        "cpuset=5,6,8}",

        "1-8",
        "main={count=0},rep={count=0},recv,ldm={count=4,cpubind=1,4,5,6}", "OK",
        "main={count=0},ldm={cpubind=1},ldm={cpubind=4},ldm={cpubind=5},ldm={"
        "cpubind=6},recv={cpubind=2},rep={count=0}",

        "1-7",
        "main={count=0},rep={count=0},recv,ldm={count=4,cpubind=1,4,5,6},tc,tc",
        "OK",
        "main={count=0},ldm={cpubind=1},ldm={cpubind=4},ldm={cpubind=5},ldm={"
        "cpubind=6},recv={cpubind=2},rep={count=0},tc={cpubind=3},tc={cpubind="
        "7}",

        "1-6",
        "main={count=0},rep={count=0},recv,ldm={count=4,cpubind=1,4,5,6},tc",
        "OK",
        "main={count=0},ldm={cpubind=1},ldm={cpubind=4},ldm={cpubind=5},ldm={"
        "cpubind=6},recv={cpubind=2},rep={count=0},tc={cpubind=3}",

        "1-6",
        "main={count=0},rep={count=0},recv,ldm={count=4,cpubind=1,4,5,6},tc,tc",
        "FAIL",
        "Too few CPU's specifed with LockExecuteThreadToCPU. This is not "
        "supported when using multiple TC threads",

        "1-5", "tc", "OK",
        "main={cpubind=1},ldm={cpubind=2},recv={cpubind=3},rep={cpubind=4},tc={"
        "cpubind=5}",

        /* order does not matter */
        "1-4", "main, ldm", "OK",
        "main={cpubind=1},ldm={cpubind=2},recv={cpubind=3},rep={cpubind=4}",

        "1-4", "ldm, main", "OK",
        "main={cpubind=1},ldm={cpubind=2},recv={cpubind=3},rep={cpubind=4}",

        "1-5", "main={count=0}", "FAIL",
        "Can't set a rep thread without a main thread.",

        "1-2", "main={count=0},rep={count=0}", "OK",
        "main={count=0},ldm={cpubind=1},recv={cpubind=2},rep={count=0}",

        // END
        nullptr};

    for (unsigned i = 0; t[i]; i += 4) {
      THRConfig tmp;
      tmp.setLockExecuteThreadToCPU(t[i + 0]);
      const int _res = tmp.do_parse(t[i + 1], 0, 0);
      const int expect_res = strcmp(t[i + 2], "OK") == 0 ? 0 : -1;
      const int res = _res == expect_res ? 0 : -1;
      int ok = expect_res == 0 ? strcmp(tmp.getConfigString(), t[i + 3]) == 0
                               : strcmp(tmp.getErrorMessage(), t[i + 3]) == 0;
      printf("mask: %s conf: %s => %s(%s) - %s - %s\n", t[i + 0], t[i + 1],
             _res == 0 ? "OK" : "FAIL", _res == 0 ? "" : tmp.getErrorMessage(),
             tmp.getConfigString(), ok == 1 ? "CORRECT" : "INCORRECT");

      OK(res == 0);
      OK(ok == 1);
    }
  }

  for (Uint32 i = 9; i < 48; i++) {
    Uint32 t, l, s, r;
    computeThreadConfig(i, t, l, s, r);
    printf(
        "MaxNoOfExecutionThreads: %u lqh: %u tc: %u send: %u recv: %u main: 1 "
        "rep: 1 => sum: %u\n",
        i, l, t, s, r, 2 + l + t + s + r);
  }

  ndb_end(0);
  return 1;
}

#endif
#if 0

/**
 * This C-program was written by Mikael Ronstrom to
 *  produce good distribution of threads, given MaxNoOfExecutionThreads
 *
 * Good is based on his experience experimenting/benchmarking
 */
#include <stdio.h>

#define Uint32 unsigned int
#define TC_THREAD_INDEX 0
#define SEND_THREAD_INDEX 1
#define RECV_THREAD_INDEX 2
#define LQH_THREAD_INDEX 3
#define MAIN_THREAD_INDEX 4
#define REP_THREAD_INDEX 5

#define NUM_CHANGE_INDEXES 3
#define NUM_INDEXES 6

static double mult_factor[NUM_CHANGE_INDEXES];

static void
set_changeable_thread(Uint32 num_threads[NUM_INDEXES],
                      double float_num_threads[NUM_CHANGE_INDEXES],
                      Uint32 index)
{
  num_threads[index] = (Uint32)(float_num_threads[index]);
  float_num_threads[index] -= num_threads[index];
}

static Uint32
calculate_total(Uint32 num_threads[NUM_INDEXES])
{
  Uint32 total = 0;
  Uint32 i;
  for (i = 0; i < NUM_INDEXES; i++)
  {
    total += num_threads[i];
  }
  return total;
}

static Uint32
find_min_index(double float_num_threads[NUM_CHANGE_INDEXES])
{
  Uint32 min_index = 0;
  Uint32 i;
  double min = float_num_threads[0];

  for (i = 1; i < NUM_CHANGE_INDEXES; i++)
  {
    if (min > float_num_threads[i])
    {
      min = float_num_threads[i];
      min_index = i;
    }
  }
  return min_index;
}

static Uint32
find_max_index(double float_num_threads[NUM_CHANGE_INDEXES])
{
  Uint32 max_index = 0;
  Uint32 i;
  double max = float_num_threads[0];

  for (i = 1; i < NUM_CHANGE_INDEXES; i++)
  {
    if (max < float_num_threads[i])
    {
      max = float_num_threads[i];
      max_index = i;
    }
  }
  return max_index;
}

static void
add_thread(Uint32 num_threads[NUM_INDEXES],
           double float_num_threads[NUM_CHANGE_INDEXES])
{
  Uint32 i;
  Uint32 max_index = find_max_index(float_num_threads);
  num_threads[max_index]++;
  float_num_threads[max_index] -= (double)1;
  for (i = 0; i < NUM_CHANGE_INDEXES; i++)
    float_num_threads[i] += mult_factor[i];
}

static void
remove_thread(Uint32 num_threads[NUM_INDEXES],
              double float_num_threads[NUM_CHANGE_INDEXES])
{
  Uint32 i;
  Uint32 min_index = find_min_index(float_num_threads);
  num_threads[min_index]--;
  float_num_threads[min_index] += (double)1;
  for (i = 0; i < NUM_CHANGE_INDEXES; i++)
    float_num_threads[i] -= mult_factor[i];
}

static void
define_num_threads_per_type(Uint32 max_no_exec_threads,
                            Uint32 num_threads[NUM_INDEXES])
{
  Uint32 total_threads;
  Uint32 num_lqh_threads;
  Uint32 i;
  double float_num_threads[NUM_CHANGE_INDEXES];

  /* Baseline to start calculations at */
  num_threads[MAIN_THREAD_INDEX] = 1; /* Fixed */
  num_threads[REP_THREAD_INDEX] = 1; /* Fixed */
  num_lqh_threads = (max_no_exec_threads / 4) * 2;
  if (num_lqh_threads > 32)
    num_lqh_threads = 32;
  switch (num_lqh_threads)
  {
    case 4:
    case 6:
    case 8:
    case 10:
    case 12:
    case 16:
    case 20:
    case 24:
    case 32:
      break;
    case 14:
      num_lqh_threads = 12;
      break;
    case 22:
      num_lqh_threads = 20;
      break;
    case 18:
      num_lqh_threads = 16;
      break;
    case 26:
    case 28:
    case 30:
      num_lqh_threads = 24;
      break;
  }
  num_threads[LQH_THREAD_INDEX] = num_lqh_threads;

  /**
   * Rest of calculations are about calculating number of tc threads,
   * send threads and receive threads based on this input.
   * We do this by calculating a floating point number and using this to
   * select the next thread group to have one more added/removed.
   */
  mult_factor[TC_THREAD_INDEX] = 0.465;
  mult_factor[SEND_THREAD_INDEX] = 0.19;
  mult_factor[RECV_THREAD_INDEX] = 0.215;
  for (i = 0; i < NUM_CHANGE_INDEXES; i++)
    float_num_threads[i] = 0.5 + (mult_factor[i] * num_lqh_threads);

  set_changeable_thread(num_threads, float_num_threads, TC_THREAD_INDEX);
  set_changeable_thread(num_threads, float_num_threads, SEND_THREAD_INDEX);
  set_changeable_thread(num_threads, float_num_threads, RECV_THREAD_INDEX);

  total_threads = calculate_total(num_threads);

  while (total_threads != max_no_exec_threads)
  {
    if (total_threads < max_no_exec_threads)
      add_thread(num_threads, float_num_threads);
    else
      remove_thread(num_threads, float_num_threads);
    total_threads = calculate_total(num_threads);
  }
}

int main(int argc, char *argv)
{
  Uint32 num_threads[NUM_INDEXES];
  Uint32 i;

  ndb_init();

  printf("MaxNoOfExecutionThreads,LQH,TC,send,recv\n");
  for (i = 9; i <= 72; i++)
  {
    define_num_threads_per_type(i, num_threads);
    printf("{ %u, %u, %u, %u, %u },\n",
           i,
           num_threads[LQH_THREAD_INDEX],
           num_threads[TC_THREAD_INDEX],
           num_threads[SEND_THREAD_INDEX],
           num_threads[RECV_THREAD_INDEX]);
  }
  ndb_end(0);
  return 0;
}

#endif
