/*
 Copyright (c) 2011, Oracle and/or its affiliates. All rights
 reserved.
 
 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; version 2 of
 the License.
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */
#include <stdio.h>
#include <assert.h>

#include "config.h"
#include "atomics.h"

#include "all_tests.h"

Uint32 engine_cas_hi = 0x717530;
ndbmc_atomic32_t engine_cas_lo = 0xb0000065;

void worker_set_cas(int verbose, Uint64 *cas);


int test_cas_bitshifts(QueryPlan *, Ndb *, int v) {
  Uint64 cas = 0ULL;
  worker_set_cas(v, &cas); 
  require(cas == 0x00717530B0000065ULL);
  worker_set_cas(v, &cas); 
  worker_set_cas(v, &cas);
  worker_set_cas(v, &cas);
  worker_set_cas(v, &cas);
  worker_set_cas(v, &cas);
  require(cas == 31935524339974250ULL);
  pass;
}


void worker_set_cas(int verbose, Uint64 *cas) {  
  bool did_inc;
  Uint32 cas_lo;
  Uint32 & cas_hi = engine_cas_hi;
  do {
    cas_lo = engine_cas_lo;    
    did_inc = atomic_cmp_swap_int(& engine_cas_lo, cas_lo, cas_lo + 1);
  } while(! did_inc);
  *cas = Uint64(cas_lo) | (Uint64(cas_hi) << 32);
  detail(verbose, "%llu \n", (unsigned long long) *cas);
}

