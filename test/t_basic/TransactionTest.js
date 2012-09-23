/*
 Copyright (c) 2012, Oracle and/or its affiliates. All rights
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

/***** Test idle state ***/
t1 = new harness.ConcurrentTest("testIdle");
t1.run = function() {
  var testCase = this;
  // idle state
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    if (tx.state.name !== 'Idle') {
      testCase.fail(new Error('Transaction should start in idle state. Actual: ' +
          tx.state.name));
      return;
    }
    if (tx.isActive() !== false) {
      testCase.fail(new Error('Idle transaction should not be active.'));
      return;
    }
    if (tx.getRollbackOnly() !== false) {
      testCase.fail(new Error('Idle transaction should not be rollback only.'));
      return;
    }
    testCase.pass();
  });
};

/***** Test active state ***/
t2 = new harness.ConcurrentTest("testActive");
t2.run = function() {
  var testCase = this;
  // idle state
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    tx.begin();
    if (tx.state.name !== 'Active') {
      testCase.fail(new Error('Transaction after begin should be in active state. Actual: ' +
          tx.state.name));
      return;
    }
    if (tx.isActive() !== true) {
      testCase.fail(new Error('Active transaction should be active.'));
      return;
    }
    if (tx.getRollbackOnly() !== false) {
      testCase.fail(new Error('Active transaction should not be rollback only.'));
      return;
    }
    testCase.pass();
  });
};

/***** Test rollback only state ***/
t3 = new harness.ConcurrentTest("testRollbackOnly");
t3.run = function() {
  var testCase = this;
  // rollback only state
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    tx.begin();
    tx.setRollbackOnly();
    if (tx.state.name !== 'RollbackOnly') {
      testCase.fail(new Error('Transaction after setRollbackOnly should be in rollback only state. Actual: ' +
          tx.state.name));
      return;
    }
    if (tx.isActive() !== true) {
      testCase.fail(new Error('RollbackOnly transaction should be active.'));
      return;
    }
    if (tx.getRollbackOnly() !== true) {
      testCase.fail(new Error('RollbackOnly transaction should be rollback only.'));
      return;
    }
    tx.setRollbackOnly();
    if (tx.state.name !== 'RollbackOnly') {
      testCase.fail(new Error('Rollback only after setRollbackOnly should be in rollback only state. Actual: ' +
          tx.state.name));
      return;
    }
    testCase.pass();
  });
};


/***** Test begin with begin ***/
t4 = new harness.ConcurrentTest("testBeginBegin");
t4.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    try {
      tx.commit();
      testCase.fail(new Error('Commit without begin should fail.'));
    } catch (err) {
      testCase.pass();
    }
  });
};

/***** Test commit without begin ***/
t5 = new harness.ConcurrentTest("testCommitWithoutBegin");
t5.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    try {
      tx.commit();
      testCase.fail(new Error('Commit without begin should fail.'));
    } catch (err) {
      testCase.pass();
    }
  });
};

/***** Test rollback without begin ***/
t6 = new harness.ConcurrentTest("testRollbackWithoutBegin");
t6.run = function() {
  var testCase = this;
  var t6OnRollback = function(err) {
    if (err) {
      // good
      try {
        tx.rollback();
        testCase.fail(new Error('Rollback without begin should fail.'));
      } catch (err) {
        testCase.pass();
      }
    } else {
      // failed to signal an error
      testCase.fail(new Error('Rollback without begin should fail.'));
    }
  };
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    tx.rollback(t6OnRollback);
  });
};


/***** Test commit with rollback only ***/
t7 = new harness.ConcurrentTest("testCommitWithRollbackOnly");
t7.run = function() {
  var testCase = this;
  var t7OnCommit = function(err) {
    if (err) {
      // good
      try {
        tx.rollback();
        testCase.fail(new Error('Commit with RollbackOnly and no callback should throw.'));
      } catch (err) {
        testCase.pass();
      }
    } else {
      // failed to signal an error
      testCase.fail(new Error('Commit with RollbackOnly should fail.'));
    }
  };
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    tx.begin();
    tx.setRollbackOnly();
    tx.commit(t7OnCommit);
  });
};


/***** Test begin with rollback only ***/
t8 = new harness.ConcurrentTest("testBeginWithRollbackOnly");
t8.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    tx.begin();
    tx.setRollbackOnly();
    try {
      tx.begin();
      testCase.fail(new Error('Begin with rollback only should fail.'));
    } catch (err) {
      testCase.pass();
    }
  });
};


/***** Test setRollbackOnly without begin ***/
t9 = new harness.ConcurrentTest("testSetRollbackOnlyWithoutBegin");
t9.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    try {
      tx.setRollbackOnly();
      testCase.fail(new Error('SetRollbackOnly without begin should fail.'));
    } catch (err) {
      testCase.pass();
    }
  });
};

t10 = new harness.SerialTest("testCommit");
t10.run = function() {
  var testCase = this;
  
  var t10OnFind = function(err, found) {
    if (found.id === 1000) {
      testCase.pass();  
    } else {
      testCase.fail('Failed to find 1000');
    }
  };
  
  var t10OnCommit = function(err, session, persisted) {
    session.find(t_basic, 1000, fail_verify_t_basic, 1000, testCase, false); // should succeed
  };
  
  var t10OnPersist = function(err, session, tx, persisted) {
    tx.commit(t10OnCommit, session, persisted);
  };
  
  var t10OnSession = function(session) {
    var tx = session.currentTransaction();
    tx.begin();
    var instance = new t_basic(1000, "Employee 1000", 1000, 1000);
    session.persist(instance, t10OnPersist, session, tx, instance);
  };
  fail_openSession(testCase, t10OnSession);
};

t11 = new harness.SerialTest("testRollback");
t11.run = function() {
  var testCase = this;
  
  var t11OnClose = function(err, found, session) {
    if (found === null) {
      testCase.pass();  
    } else {
      testCase.fail('Found 1100 which was supposed to be rolled back.');
    }
  };
  var t11OnFind = function(err, found, session) {
    session.close(t11OnClose, found, session);
  };
  
  var t11OnRollback = function(err, session, persisted) {
    if (err) {
      testCase.fail(err);
    } else {
      session.find(t_basic, 1100, t11OnFind, session); // should fail
    }
  };
  
  var t11OnPersist = function(err, session, tx, persisted) {
    tx.rollback(t11OnRollback, session, persisted);
  };
  
  var t11OnSession = function(session) {
    var tx = session.currentTransaction();
    tx.begin();
    var instance = new t_basic(1100, "Employee 1100", 1100, 1100);
    session.persist(instance, t11OnPersist, session, tx, instance);
  };
  fail_openSession(testCase, t11OnSession);
};

module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8, t9, t10, t11];
