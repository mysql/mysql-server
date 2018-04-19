/*
 Copyright (c) 2011, 2017, Oracle and/or its affiliates. All rights reserved.
 
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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */
#include "my_config.h"
#include <stdio.h>

#include <NdbApi.hpp> 

#include "QueryPlan.h"
#include "Operation.h"

#define require(x) if(!(x)) return __LINE__;
#define pass return 0;

#define detail(v, ...) if(v) printf (__VA_ARGS__)
#define RESULT getNdbError().code

#define REQ_NONE           0
#define REQ_NDB_CONNECTION 1
#define REQ_DEMO_TABLE     2

void delete_row(QueryPlan *plan, Ndb *db, const char * key, int verbose);

typedef int TESTCASE(QueryPlan *plan, Ndb *db, int verbose);

struct test_item {
  int enabled;
  const char *name;
  TESTCASE *function;
  int requires;
};

TESTCASE run_cas_test;
TESTCASE test_cas_bitshifts;
TESTCASE run_incr_test;
TESTCASE run_allocator_test;
TESTCASE run_pool_test;
TESTCASE run_tsv_test;
TESTCASE run_queue_test;
TESTCASE run_lookup_test;

#ifdef HARNESS

struct test_item all_tests[] = { 
  { 1, "cas operation",   run_cas_test,         REQ_DEMO_TABLE },
  { 1, "cas bitshifting", test_cas_bitshifts,   REQ_NONE },
  { 1, "incr operation",  run_incr_test,        REQ_DEMO_TABLE }, 
  { 1, "allocator",       run_allocator_test,   REQ_NONE },
  { 0, "pool",            run_pool_test,        REQ_NDB_CONNECTION },
  { 1, "tsv",             run_tsv_test,         REQ_NONE },
  { 1, "queue",           run_queue_test,       REQ_NONE },
  { 1, "lookup table",    run_lookup_test,      REQ_NONE },
  { 0, NULL, NULL, 0 }
};


const char * requirements[3] = 
{ 
  "none", "ndb connection", "demo_table" 
};

#else

extern char * connect_string;

#endif
