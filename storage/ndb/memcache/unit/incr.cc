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
#include "all_tests.h"

int do_incr(int v, QueryPlan *plan, Ndb *db, 
            const char *akey, bool, bool, Uint64 *val);

int run_incr_test(QueryPlan *plan, Ndb *db, int v) {
  int r = 0;
  Uint64 val = 33;
  
  delete_row(plan, db, "incr_unit_test_1", v);
  delete_row(plan, db, "incr_unit_test_2", v);
  
  detail(v, "Test 1: INCR non-existing row, create=false\n");
  r = do_incr(v, plan, db, "incr_unit_test_1", false, true, &val);
  detail(v, "Result - NDB=%d Val=%llu \n\n", r, (long long unsigned) val);
  require(r == 626);
  require(val == 33);

  detail(v, "Test 2: INCR non-existing row, create=true, update = false\n");
  r = do_incr(v, plan, db, "incr_unit_test_1", true, false, &val);
  detail(v, "Result - NDB=%d Val=%llu \n\n", r, val);
  require(r == 626); // the transaction gets a 626 even if the insert succeeds
  require(val == -1ULL);

  detail(v, "test 3: READ row created in test 2\n");
  r = do_incr(v, plan, db, "incr_unit_test_1", false, false, &val);
  detail(v, "Result - NDB=%d Val=%llu \n\n", r, (long long unsigned) val);
  require(r == 0);
  require(val == -1ULL);

  detail(v, "Test 4: INCR non-existing row, create=true\n");
  r = do_incr(v, plan, db, "incr_unit_test_2", true, true, &val);
  detail(v, "Result - NDB=%d Val=%llu \n\n", r, (long long unsigned) val);
  require(r == 626);
  require(val == 0);
  
  detail(v, "Test 5: INCR existing row, create=false\n");
  r = do_incr(v, plan, db, "incr_unit_test_2", false, true, &val);
  detail(v, "Result - NDB=%d Val=%llu \n\n", r, (long long unsigned) val);
  require(r == 0);
  require(val == 1);  

  detail(v, "Test 6: INCR existing row, create=true\n");
  r = do_incr(v, plan, db, "incr_unit_test_2", true, true, &val);
  detail(v, "Result - NDB=%d Val=%llu \n\n", r, (long long unsigned) val);
  require(r == 630);   // the insert failed but the update succeeded
  require(val == 2);
   
  pass;
}


int do_incr(int v, QueryPlan *plan, Ndb *db, const char *akey, 
            bool create, bool update, Uint64 *val) {
  int r;
  char key[50];
  char ndbkeybuffer[300];
  char ndbrowbuffer1[16384];
  char ndbrowbuffer2[16384];
  
  strcpy(key, akey);
  
  Operation op1(plan, OP_READ, ndbkeybuffer); 
  Operation op(plan); 

  op.key_buffer = ndbkeybuffer; 
  op.buffer = ndbrowbuffer1;
  const NdbOperation *ndbop1 = 0;
  const NdbOperation *ndbop2 = 0;
  const NdbOperation *ndbop3 = 0;
  
  /* Set the Key */
  op.clearKeyNullBits();
  op.setKeyPart(COL_STORE_KEY, key, strlen(key));
  
  /* Set the Row */
  op.clearNullBits();
  op.setColumn(COL_STORE_KEY, key, strlen(key));
  op.setColumnBigUnsigned(COL_STORE_CAS, 0ULL);

  NdbTransaction *tx = op.startTransaction(db);
  if(! tx) {
    int r =  db->RESULT;
    detail(v, " get tx: %d \n", r);
    return r;
  }
  
  // NdbOperation #1: read
  {
    op1.buffer = ndbrowbuffer2;
    NdbOperation::LockMode lmod = NdbOperation::LM_Exclusive;
    
    ndbop1 = op1.readTuple(tx, lmod);
    if(! ndbop1) {
      detail(v, "  op 1 error: %d \n", tx->getNdbError().code);
      tx->close();
      return -1;
    }
  }
  
  
  // NdbOperation #2:   insert
  // If the create flag is set, create a record with "initial - delta", i.e. -1
  if(create) {    
    NdbOperation::OperationOptions options;
    op.setColumnBigUnsigned(COL_STORE_MATH, -1ULL);
    options.optionsPresent = NdbOperation::OperationOptions::OO_ABORTOPTION;  
    options.abortOption = NdbOperation::AO_IgnoreError;
    ndbop2 = op.insertTuple(tx, & options); 
    if(! ndbop2) {
      detail(v, "  op 2 error: %d \n", tx->RESULT);
      tx->close();
      return(-2);
    }
  }
  
  // NdbOperation #3:  update (interpreted)
  if (update) {
    NdbOperation::OperationOptions options;
    const Uint32 program_size = 10;
    Uint32 program[program_size];
    NdbInterpretedCode incr_code(plan->table, program, program_size);
    
    if(incr_code.add_val(plan->math_column_id, (Uint32) 1)) return -4;
    if(incr_code.interpret_exit_ok())                       return -5;
    if(incr_code.finalise())                                return -6;
    
    options.optionsPresent = NdbOperation::OperationOptions::OO_INTERPRETED;
    options.interpretedCode = & incr_code;

    ndbop3 = op.updateTuple(tx,  & options);
    if(! ndbop3) {
      detail(v,"  op 3 error: %d \n", tx->getNdbError().code);
      tx->close();
      return(-3);
    }
    
  }
  
  tx->execute(NdbTransaction::Commit);
  r = tx->getNdbError().code;
  detail(v, "    transaction: %d", r);
  if(ndbop1) detail(v, "    read: %d", ndbop1->RESULT);
  if(ndbop2) detail(v, "    insert: %d", ndbop2->RESULT);
  if(ndbop3) detail(v, "    update: %d", ndbop3->RESULT);
  detail(v, "\n");
  
  if(ndbop1->RESULT == 0) 
    *val = op1.getBigUnsignedValue(COL_STORE_MATH) + (update ? 1 : 0);
  else if(create)
    *val = -1ULL + (update ? 1 : 0);
  
  tx->close();
  return r;
}

