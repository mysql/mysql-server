/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
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

var meta = mynode.meta;
var udebug = unified_debug.getLogger("db.InsertTest.js");
function Counter() {
  this.number = 0;
}
Counter.prototype.next = function() {
  return ++this.number;
};

function verify(err, instance, id, testCase, domainObject) {
  if (err) {
    testCase.fail(err);
    return;
  }
  if (typeof(instance) !== 'object') {
    testCase.fail(new Error('Result for id ' + id + ' is not an object; actual type: ' + typeof(instance)));
    return;
  }
  if (instance === null) {
    testCase.fail(new Error('Result for id ' + id + ' is null.'));
    return;
  }
  if (domainObject) {
    if (typeof(instance.getAge) !== 'function') {
      testCase.fail(new Error('Result for id ' + id + ' is not a domain object'));
      return;
    }
  }
  udebug.log_detail('InsertTest.verify id:', id, 'instance:', instance);
  var message = '';
  if (instance.id != id) {
    message += 'fail to verify id: expected: ' + id + ', actual: ' + instance.id + '\n';
  }
  if (instance.age != id) {
    message += 'fail to verify age: expected: ' + id + ', actual: ' + instance.age + '\n';
  }
  if (instance.magic != id) {
    message += 'fail to verify magic: expected: ' + id + ', actual: ' + instance.magic + '\n';
  }
  if (instance.name !== "Employee " + id) {
    message += 'fail to verify name: expected: ' + "Employee " + id + ', actual: ' + instance.name + '\n';
  }
  if (message !== '') {
    testCase.appendErrorMessage(message);
  }
  testCase.failOnError();
}

/***** insert with existing schema and literal ***/
var t1 = new harness.SerialTest("testDbInsertWithExistingSchemaAndLiteral");
t1.run = function() {
  var testCase = this;
  var db;
  var count = 0;
  // create the literals
  var object1 = {id:4071, name:'Employee 4071', age:4071, magic:4071}; ++count;
  var object2 = {id:4072, name:'Employee 4072', age:4072, magic:4072}; ++count;
  var object3 = {id:4073, name:'Employee 4073', age:4073, magic:4073}; ++count;
  var object4 = {id:4074, name:'Employee 4074', age:4074, magic:4074}; ++count;
  var object5 = {id:4075, name:'Employee 4075', age:4075, magic:4075}; ++count;
  function check(err) {
    if (err) testCase.appendErrorMessage(err.message);
    if (--count == 0) {
      // all done with insert
      testCase.failOnError();
    }
  }
  
  fail_connect(testCase, function(sessionFactory) {
    db = sessionFactory.db();
    db.db_basic.insert(object1, check);
    db.db_basic.insert(object2, check);
    db.db_basic.insert(object3, check);
    db.db_basic.insert(object4, check);
    db.db_basic.insert(object5, check);
    
  });
};

/***** insert with non-existing schema and literal ***/
var t2 = new harness.SerialTest("testDbInsertWithNonExistingSchemaAndLiteral");
t2.run = function() {
  var testCase = this;
  var db;
  // create the literal 4081
  var object = {id:4081, name:'Employee 4081', age:4081, magic:4081};
  fail_connect(testCase, function(sessionFactory) {
    db = sessionFactory.db();
    testCase.new_schemas = [];
    testCase.new_schemas.push('db_new_schema1');
    db.db_new_schema1.insert(object, function(err) {
      if (err) {
        testCase.fail(err);
      } else {
        // verify that the object was inserted into the database
        sessionFactory.openSession(null, function(err, session) {
          if (err) {
            testCase.fail(err);
          } else {
            testCase.session = session;
            // verify will end the test case
            session.find('db_new_schema1', 4081, verify, 4081, testCase, false);
          }
        });
      }
    });
  });
};

/***** db.insert with table mapping ***/
var t3 = new harness.SerialTest("testDbInsertWithMetaTableMappingAndLiteral");
t3.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('db_new_schema2', meta.index('cage'), meta.uniqueIndex('cmagic'));
  tableMapping.mapField('id', 'cid', meta.int(32).primaryKey());
  tableMapping.mapField('name', 'cname', meta.varchar(32));
  tableMapping.mapField('age', 'cage', meta.int(32));
  tableMapping.mapField('magic', 'cmagic', meta.int(32));
  // create the literal 4091
  var object = {id:4091, name:'Employee 4091', age:4091, magic:4091};
  fail_openSession(testCase, function(session) {
    session.sessionFactory.mapTable(tableMapping);
    var db = session.sessionFactory.db();
    db.db_new_schema2.insert(object, function(err) {
      if (err) {
        testCase.fail(err);
      } else {
        // verify will end the test case
        testCase.session.find('db_new_schema2', 4091, verify, 4091, testCase, false);
      }
    });
  });
};

/***** persist with user-defined table mapping ***/
var t4 = new harness.SerialTest("testPersistWithMetaTableMappingAndLiteral");
t4.run = function() {
  var testCase = this;
  var tableMapping = new mynode.TableMapping('db_new_schema3');
  tableMapping.mapField('id', 'cid', meta.int(32).primaryKey());
  tableMapping.mapField('name', 'cname', meta.varchar(32));
  tableMapping.mapField('age', 'cage', meta.int(32));
  tableMapping.mapField('magic', 'cmagic', meta.int(32).unique());
  // create the literal 4091
  var object = {id:4091, name:'Employee 4091', age:4091, magic:4091};
  fail_openSession(testCase, function(session) {
    session.sessionFactory.mapTable(tableMapping);
    session.persist('db_new_schema3', object, function(err) {
      if (err) {
        testCase.fail(err);
      } else {
        // verify will end the test case
        session.find('db_new_schema3', 4091, verify, 4091, testCase, false);
      }
    });
  });
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1, t2, t3, t4];
