/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

var udebug     = unified_debug.getLogger("test/t_basic/DefaultValuesTest.js");

/***** Persist with domain object ***/
var t1 = new harness.SerialTest("testPersistDomainObjectDefaultNameAndAge");
t1.run = function() {
  var testCase = this;
  // create the domain object 4170; default age and name
  var object = new global.t_basic(4170);
  object.magic = 4170;
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      session2.find(global.t_basic, 4170, function(err, row) {
        if (err) {
          testCase.appendErrorMessage('t1 transaction error ' + err);
        } else {
          testCase.errorIfNotEqual('t1 name', 'Employee 666', row.name);
          testCase.errorIfNotEqual('t1 age', null, row.age);
        }
        testCase.failOnError();
      });
    }, session);
  });
};

/***** Persist with constructor and domain object ***/
var t2 = new harness.SerialTest("testPersistConstructorAndObjectDefaultNameAndAge");
t2.run = function() {
  var testCase = this;
  // create the domain object 4171; default age and name
  var object = new global.t_basic(4171);
  object.magic = 4171;
  fail_openSession(testCase, function(session) {
    session.persist(global.t_basic, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      session2.find(global.t_basic, 4171, function(err, row) {
        if (err) {
          testCase.appendErrorMessage('t2 transaction error ' + err);
        } else {
          testCase.errorIfNotEqual('t2 name', 'Employee 666', row.name);
          testCase.errorIfNotEqual('t2 age', null, row.age);
        }
        testCase.failOnError();
      });
    }, session);
  });
};

/***** Persist with table name and domain object ***/
var t3 = new harness.SerialTest("testPersistTableNameAndObjectDefaultNameAndAge");
t3.run = function() {
  var testCase = this;
  // create the domain object 4172; default name and age
  var object = new global.t_basic(4172);
  object.magic = 4172;
  fail_openSession(testCase, function(session) {
    session.persist('t_basic', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      session2.find(global.t_basic, 4172, function(err, row) {
        if (err) {
          testCase.appendErrorMessage('t3 transaction error ' + err);
        } else {
          testCase.errorIfNotEqual('t3 name', 'Employee 666', row.name);
          testCase.errorIfNotEqual('t3 age', null, row.age);
        }
        testCase.failOnError();
      });
    }, session);
  });
};

/***** Persist with constructor and literal ***/
var t4 = new harness.SerialTest("testPersistConstructorAndLiteralDefaultNameAndAge");
t4.run = function() {
  var testCase = this;
  // create the literal 4173; default name and age
  var object = {id: 4173, magic: 4173};
  fail_openSession(testCase, function(session) {
    session.persist(global.t_basic, object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      session2.find(global.t_basic, 4173, function(err, row) {
        if (err) {
          testCase.appendErrorMessage('t4 transaction error ' + err);
        } else {
          testCase.errorIfNotEqual('t4 name', 'Employee 666', row.name);
          testCase.errorIfNotEqual('t4 age', null, row.age);
        }
        testCase.failOnError();
      });
    }, session);
  });
};

/***** Persist with table name and literal ***/
var t5 = new harness.SerialTest("testPersistTableNameAndLiteralDefaultNameAndAge");
t5.run = function() {
  var testCase = this;
  // create the literal 4174; default name and age
  var object = {id: 4174, magic: 4174};
  fail_openSession(testCase, function(session) {
    session.persist('t_basic', object, function(err, session2) {
      if (err) {
        testCase.fail(err);
        return;
      }
      session2.find(global.t_basic, 4174, function(err, row) {
        if (err) {
          testCase.appendErrorMessage('t5 transaction error ' + err);
        } else {
          testCase.errorIfNotEqual('t5 name', 'Employee 666', row.name);
          testCase.errorIfNotEqual('t5 age', null, row.age);
        }
        testCase.failOnError();
      });
    }, session);
  });
};

/***** Persist with domain object not null no default value undefined: error ***/
var t6 = new harness.SerialTest("testPersistDomainObjectNotNullNoDefaultUndefined");
t6.run = function() {
  var testCase = this;
  // create the domain object 4175 with no magic; must fail
  var object = new global.t_basic(4175);
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err, session2) {
      if (!err) {
        testCase.appendErrorMessage('t6 Expected error not received for not null no default undefined');
      } else {
        udebug.log('t6 Expected error received:', err);
      }
      testCase.failOnError();
    }, session);
  });
};

/***** Persist with domain object not null no default value null: error ***/
var t7 = new harness.SerialTest("testPersistDomainObjectNotNullNoDefaultNull");
t7.run = function() {
  var testCase = this;
  // create the domain object 4176 with magic null; must fail
  var object = new global.t_basic(4176);
  object.magic = null;
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err, session2) {
      if (!err) {
        testCase.appendErrorMessage('t7 Expected error not received for not null no default null');
      } else {
        testCase.errorIfNotEqual('t7 Wrong sqlstate', '23000', err.sqlstate);
        udebug.log('t7 Expected error received:', err);
      }
      testCase.failOnError();
    }, session);
  });
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4, t5, t6, t7];
