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

/** The t_basic domain object */
global.converter = function(id, name, status, magic) {
  if (typeof id !== 'undefined') this.id = id;
  if (typeof name !== 'undefined') this.name = name;
  if (typeof status !== 'undefined') this.status = status;
  if (typeof magic !== 'undefined') this.magic = magic;
};
global.converter.prototype.getStatus = function() {
    return this.status;
};

var STATUS = {
    NEVER_MARRIED: {value: 0, code: 'N', name: 'NEVER_MARRIED'},
          MARRIED: {value: 1, code: 'M', name: 'MARRIED'},
         DIVORCED: {value: 2, code: 'D', name: 'DIVORCED'},
    lookup: function(value) {
      switch (value) {
      case 0: return this.NEVER_MARRIED; 
      case 1: return this.MARRIED; 
      case 2: return this.DIVORCED; 
      default: return null;
      }
    }
};

// column converter for status
var statusConverter = {
    toDB: function toDB(status) {
      return status.value;
    },
    fromDB: function fromDB(value) {
      return STATUS.lookup(value);
    }
};

//map t_basic domain object
var tablemapping = new mynode.TableMapping('test.t_basic');
tablemapping.mapAllColumns = false;
tablemapping.mapField('id');
tablemapping.mapField('status', 'age', statusConverter);
tablemapping.mapField('name');
tablemapping.mapField('magic');
tablemapping.applyToClass(global.converter);

/** Test the toDB and fromDB functions */
var t0 = new harness.ConcurrentTest('testStatusColumnConverter');
t0.run = function() {
  var testCase = this;
  this.errorIfNotEqual('toDB', 1, statusConverter.toDB(STATUS.MARRIED));
  this.errorIfNotEqual('fromDB', STATUS.DIVORCED, statusConverter.fromDB(2));
  this.failOnError();
};

/***** Persist with domain object ***/
var t1 = new harness.ConcurrentTest('testFieldColumnConverter');
t1.run = function() {
  var testCase = this;
  // create the domain object 4090
  var object = new global.converter(4090, 'Employee 4090', STATUS.MARRIED, 4090);
  fail_openSession(testCase, function(session) {
    session.persist(object, function(err) {
      if (err) {
        testCase.fail(err);
        return;
      }
      // key and testCase are passed to fail_verify_t_basic as extra parameters
      session.find(global.converter, 4090, function(err, instance) {
        var id = 4090;
        if (err) {
          testCase.fail(err);
        } else {
          if (typeof(instance) !== 'object') {
            testCase.appendErrorMessage('Result for id ' + id + ' is not an object; actual type: ' + typeof(instance));
          } else {
            if (instance === null) {
              testCase.appendErrorMessage('Result for id ' + id + ' is null.');
            } else {
              if (typeof(instance.getStatus) !== 'function') {
              testCase.appendErrorMessage('Result for id ' + id + ' is not a domain object');
            }
            testCase.errorIfNotEqual('fail to verify id', id, instance.id);
            testCase.errorIfNotEqual('fail to verify status', STATUS.MARRIED, instance.status);
            testCase.errorIfNotEqual('fail to verify name', 'Employee ' + id, instance.name);
            testCase.errorIfNotEqual('fail to verify magic', id, instance.magic);
          }
          }
        }
        testCase.failOnError();
      });
    });
  });
};

var t2 = new harness.ConcurrentTest('testPerformance');
t2.converter = {
  toDB: function toDB(value) {
    return value;
  },
  fromDB: function fromDB(dbValue) {
    return dbValue;
  }
};

/** Always use convert function */
t2.convert1 = function(value) {
  return t2.converter.toDB(value);
};

/** Only use convert function if it is defined */
t2.convert2 = function(value) {
  if (t2.converter999) {
    return t2.converter999.toDB(value);
  }
  return value;
};

/** Only use convert function if it is not undefined */
t2.convert3 = function(value) {
  if (typeof t2.converter999 !== 'undefined') {
    return t2.converter999.toDB(value);
  }
  return value;
};

/** Only use convert function if it is a function */
t2.convert4 = function(value) {
  if (typeof t2.converter999 === 'function') {
    return t2.converter999.toDB(value);
  }
  return value;
};

t2.Timer = function(name, iterations) {
  this.name = name;
  this.startTime = Date.now();
  console.log(this.startTime);
  this.iterations = iterations;
};
t2.Timer.prototype.stop = function() {
  var now = Date.now();
  console.log(now);
  var elapsed = Date.now() - this.startTime;
  var average = elapsed * 1000000 / this.iterations;
  console.log(this.name, 'elapsed time in milliseconds', elapsed, 'average microseconds per iteration', average);
};

t2.run = function() {
  var testCase = this;
  var i, numberOfIterations = 100000000;
  var r, numberOfRuns = 5;
  var value;
  // test performance of empty converter
  for (r = 0; r < numberOfRuns; ++r) {
    var timer1 = new this.Timer('                  empty converter', numberOfIterations);
    for (i = 0; i < numberOfIterations; ++i) {
      value = this.convert1(i);
    }
    timer1.stop();
  }
  for (r = 0; r < numberOfRuns; ++r) {
    var timer2 = new this.Timer('                   if (converter)', numberOfIterations);
    // test performance of if then converter
    for (i = 0; i < numberOfIterations; ++i) {
      value = this.convert2(i);
    }
    timer2.stop();
  }
  for (r = 0; r < numberOfRuns; ++r) {
    var timer3 = new this.Timer('if typeof converter !== undefined', numberOfIterations);
    // test performance of if then converter
    for (i = 0; i < numberOfIterations; ++i) {
      value = this.convert3(i);
    }
    timer3.stop();
  }
  for (r = 0; r < numberOfRuns; ++r) {
    var timer4 = new this.Timer(' if typeof converter === function', numberOfIterations);
    // test performance of if then converter
    for (i = 0; i < numberOfIterations; ++i) {
      value = this.convert4(i);
    }
    timer4.stop();
  }
  testCase.pass();
};

/*************** EXPORT THE TOP-LEVEL GROUP ********/
module.exports.tests = [t1];
