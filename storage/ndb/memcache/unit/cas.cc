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

#include "all_tests.h"

int set_row(int, QueryPlan *, Ndb *, const char *, Uint64 , Uint64 );
void build_cas_routine(NdbInterpretedCode *, QueryPlan *plan, Uint64 *cas);



int run_cas_test(QueryPlan *plan, Ndb *db, int v) {
  const Uint64 cas = 30090000000000003ULL;
  int r;
  
  r = set_row(v, plan, db, "cas_unit_test_1", 0ULL, cas);      // a normal update
  detail(v, "(1): %d\n", r);
  require(r == 0);  

  r = set_row(v, plan, db, "cas_unit_test_1", cas, cas + 1);   // an interpreted update
  detail(v, "(2): %d\n", r);
  require(r == 0);  
  
  r = set_row(v, plan, db, "cas_unit_test_2", 0ULL, cas);      // a normal update
  detail(v, "(2): %d\n", r);
  require(r == 0);  

  r = set_row(v, plan, db, "cas_unit_test_2", cas-1, cas+1);   // this should fail  
  detail(v, "(2): %d\n", r);
  require(r == 899);  
  pass;
}


int set_row(int v, QueryPlan *plan, Ndb *db,
            const char *akey, Uint64 old_cas, Uint64 new_cas) {
  NdbOperation::OperationOptions options;
  NdbInterpretedCode code(plan->table);
  char key[50];
  char value[50];
  char ndbkeybuffer[300];
  char ndbrowbuffer[16384];
  
  strcpy(key, akey);
  strcpy(value, "munch");
  
  detail(v, "set_row: key=%s, old_cas=%llu, new_cas=%llu ", key, old_cas, new_cas);
  
  Operation op(plan);  
  op.key_buffer = ndbkeybuffer;
  op.buffer = ndbrowbuffer;
  
  /* Set the Key */
  op.clearKeyNullBits();
  op.setKeyPart(COL_STORE_KEY, key, strlen(key));
  
  /* Set the Row */
  op.clearNullBits();
  op.setColumn(COL_STORE_KEY, key, strlen(key));    
  op.setColumn(COL_STORE_VALUE, value, strlen(value));
  op.setColumnBigUnsigned(COL_STORE_CAS, new_cas);
  
  NdbTransaction *tx = op.startTransaction(db);
  
  if(old_cas) {
    /* This is an interpreted update */
    build_cas_routine(&code, plan, & old_cas);
    options.optionsPresent= NdbOperation::OperationOptions::OO_INTERPRETED;
    options.interpretedCode = & code;
    op.updateTuple(tx, &options);
  }
  else {
    /* Just a normal insert/update */
    op.writeTuple(tx);
  }
  
  tx->execute(NdbTransaction::Commit);
  int r = tx->getNdbError().code;
  tx->close();
  
  return r;
}


void build_cas_routine(NdbInterpretedCode *r, QueryPlan *plan, Uint64 *cas) {  
  
  /* Branch on cas_value != cas_column */
  /* branch_col_ne(value, NOOP, column_id, label) */
  r->branch_col_ne((const void *) cas, 0, plan->cas_column_id, 0);
  
  /* Here is the cas_value == cas_column branch: */
  r->interpret_exit_ok();                  // allow operation to succeed
  
  /* Here is the cas_value != cas_column branch: */
  r->def_label(0);
  r->interpret_exit_nok(899);           // abort the operation
  
  r->finalise();  
}
