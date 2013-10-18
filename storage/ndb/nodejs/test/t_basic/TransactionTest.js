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
"use strict";
/*jslint newcap: true */
/*global t_basic, verify_t_basic, fail_verify_t_basic */

var udebug = unified_debug.getLogger("TransactionTest.js");

if(! global.t_basic) {
  require("./lib.js");
}

/***** Test idle state ***/
var t1 = new harness.ConcurrentTest("testIdle");
t1.run = function() {
  var testCase = this;
  // idle state
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    if (tx.state.name !== 'Idle') {
      testCase.fail(new Error('t1 Transaction should start in idle state. Actual: ' +
          tx.state.name));
      return;
    }
    if (tx.isActive() !== false) {
      testCase.fail(new Error('t1 Idle transaction should not be active.'));
      return;
    }
    if (tx.getRollbackOnly() !== false) {
      testCase.fail(new Error('t1 Idle transaction should not be rollback only.'));
      return;
    }
    tx.setRollbackOnly(function(err) {
      if (!err) {
        testCase.fail(new Error('t1 Idle transaction should not allow set rollback only'));
      } else {
        tx.commit(function(err) {
          if (!err) {
            testCase.fail(new Error('t1 Idle transaction should not allow commit.'));
          } else {
            tx.rollback(function(err) {
              if (!err) {
                testCase.fail(new Error('t1 Idle transaction should not allow rollback.'));
              } else {
                testCase.pass();
              }
            });
          }
        });
      }
    });
  });
};

/***** Test active state ***/
var t2 = new harness.ConcurrentTest("testActive");
t2.run = function() {
  var testCase = this;
  // idle state
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    tx.begin();
    if (tx.state.name !== 'Active') {
      testCase.fail(new Error('t2 Transaction after begin should be in active state. Actual: ' +
          tx.state.name));
      return;
    }
    if (tx.isActive() !== true) {
      testCase.fail(new Error('t2 Active transaction should be active.'));
      return;
    }
    if (tx.getRollbackOnly() !== false) {
      testCase.fail(new Error('t2 Active transaction should not be rollback only.'));
      return;
    }
    tx.begin(function(err) {
      if (!err) {
        testCase.fail(new Error('t2 Active transaction should not allow begin.'));
      } else {
        testCase.pass();
      }
    });
  });
};

/***** Test rollback only state ***/
var t3 = new harness.ConcurrentTest("testRollbackOnly");
t3.run = function() {
  var testCase = this;
  // rollback only state
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    tx.begin();
    tx.setRollbackOnly();
    if (tx.state.name !== 'RollbackOnly') {
      testCase.fail(new Error('t3 Transaction after setRollbackOnly should be in rollback only state. Actual: ' +
          tx.state.name));
      return;
    }
    if (tx.isActive() !== true) {
      testCase.fail(new Error('t3 RollbackOnly transaction should be active.'));
      return;
    }
    if (tx.getRollbackOnly() !== true) {
      testCase.fail(new Error('t3 RollbackOnly transaction should be rollback only.'));
      return;
    }
    tx.setRollbackOnly();
    if (tx.state.name !== 'RollbackOnly') {
      testCase.fail(new Error('t3 Rollback only after setRollbackOnly should be in rollback only state. Actual: ' +
          tx.state.name));
      return;
    }
    tx.begin(function(err) {
      if (!err) {
        testCase.fail(new Error('t3 Rollback only transaction should not allow begin.'));
      } else {
        testCase.pass();
      }
    });
  });
};


/***** Test begin with begin ***/
var t4 = new harness.ConcurrentTest("testBeginBegin");
t4.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    try {
      tx.begin();
      tx.begin();
      testCase.fail(new Error('t4 Begin Begin without commit or rollback should fail.'));
    } catch (err) {
      testCase.pass();
    }
  });
};

/***** Test commit without begin with and without callback ***/
var t5 = new harness.ConcurrentTest("testCommitWithoutBegin");
t5.run = function() {
  var testCase = this;
  var tx;
  var t5OnCommit = function(err) {
    if (err) {
      // so far so good
      try {
        tx.commit();
        testCase.fail(new Error('t5 Commit without begin and without callback should throw.'));
      } catch (e) {
        testCase.pass();
      }
    } else {
      // failed to signal an error
      testCase.fail(new Error('t5 Commit without begin should fail.'));
    }
  };

  fail_openSession(testCase, function(session) {
    tx = session.currentTransaction();
    tx.commit(t5OnCommit);
  });
};

/***** Test rollback without begin with and without callback ***/
var t6 = new harness.ConcurrentTest("testRollbackWithoutBegin");
t6.run = function() {
  var tx;
  var testCase = this;
  var t6OnRollback = function(err) {
    if (err) {
      // so far so good
      try {
        tx.rollback();
        testCase.fail(new Error('t6 Rollback without begin and without callback should throw.'));
      } catch (ex) {
        testCase.pass();
      }
    } else {
      // failed to signal an error
      testCase.fail(new Error('t6 Rollback without begin should fail.'));
    }
  };
  fail_openSession(testCase, function(session) {
    tx = session.currentTransaction();
    tx.rollback(t6OnRollback);
  });
};


/***** Test commit with rollback only ***/
var t7 = new harness.ConcurrentTest("testCommitWithRollbackOnly");
t7.run = function() {
  var tx;
  var testCase = this;
  var t7OnCommit = function(err) {
    if (err) {
      // so far so good
      try {
        tx.commit();
        testCase.fail(new Error('t7 SetRollbackOnly, Commit with callback, Commit with no callback should throw.'));
      } catch (ex) {
        tx.rollback();
        testCase.pass();
      }
    } else {
      // failed to signal an error
      testCase.fail(new Error('Commit with RollbackOnly should fail.'));
    }
  };
  fail_openSession(testCase, function(session) {
    tx = session.currentTransaction();
    tx.begin();
    tx.setRollbackOnly();
    tx.commit(t7OnCommit);
  });
};


/***** Test begin with rollback only ***/
var t8 = new harness.ConcurrentTest("testBeginWithRollbackOnly");
t8.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    tx.begin();
    tx.setRollbackOnly();
    try {
      tx.begin();
      testCase.fail(new Error('t8 Begin with rollback only should fail.'));
    } catch (err) {
      testCase.pass();
    }
  });
};


/***** Test setRollbackOnly without begin ***/
var t9 = new harness.ConcurrentTest("testSetRollbackOnlyWithoutBegin");
t9.run = function() {
  var testCase = this;
  fail_openSession(testCase, function(session) {
    var tx = session.currentTransaction();
    try {
      tx.setRollbackOnly();
      testCase.fail(new Error('t9 SetRollbackOnly without begin should fail.'));
    } catch (err) {
      testCase.pass();
    }
  });
};

var t10 = new harness.SerialTest("testCommit");
t10.run = function() {
  udebug.log("t10 run");
  var testCase = this;
  
  var t10OnFind = function(err, found) {
    udebug.log("t10 OnFind");
    if (found.id === 1000) {
      testCase.pass();  
    } else {
      testCase.fail('t10 Failed to find 1000');
    }
  };
  
  var t10OnCommit = function(err, session, persisted) {
    udebug.log("t10 OnCommit");
    session.find(t_basic, 1000, fail_verify_t_basic, 1000, testCase, false); // should succeed
  };
  
  var t10OnPersist = function(err, session, tx, persisted) {
    udebug.log("t10 OnPersist");
    tx.commit(t10OnCommit, session, persisted);
  };
  
  var t10OnSession = function(session) {
    udebug.log("t10 OnSession");
    var tx = session.currentTransaction();
    tx.begin();
    var instance = new t_basic(1000, "Employee 1000", 1000, 1000);
    session.persist(instance, t10OnPersist, session, tx, instance);
  };
  fail_openSession(testCase, t10OnSession);
};

var t11 = new harness.SerialTest("testRollback");
t11.run = function() {
  var testCase = this;
  
  var t11OnClose = function(err, found, session) {
    if (found === null) {
      testCase.pass();  
    } else {
      testCase.fail('t11 Found 1100 which was supposed to be rolled back.');
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
