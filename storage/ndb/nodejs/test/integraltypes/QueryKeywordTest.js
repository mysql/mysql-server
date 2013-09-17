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

/*global unified_debug, mynode, verify_integraltypes_keyword, harness,
  fail_openSession, fail_verify_integraltypes_keyword_array 
 */
"use strict";

var udebug = unified_debug.getLogger("QueryKeywordTest.js");
var IntegraltypesKeywordId = function(id) {
  if(id !== undefined) {
    // name the id field 'where'
    this.where = id;
  }
  this.getId = function() {
    return this.where;
  };
};

var mapIntegraltypesKeyword = function() {
  // map special domain type
  var tablemapping = new mynode.TableMapping("test.integraltypes");
  tablemapping.mapField('where', 'id');
  tablemapping.mapField('execute', 'ttinyint');
  tablemapping.mapField('count', 'tsmallint');
  tablemapping.mapField('param', 'tmediumint');
  tablemapping.mapField('field', 'tint');
  tablemapping.applyToClass(IntegraltypesKeywordId);
};

/** Verify the instance or append an error message to the test case */
global.verify_integraltypes_keyword = function(err, instance, id, testCase, domainObject) {
  if (err) {
    testCase.appendErrorMessage(err);
    return;
  }
  if (typeof(instance) !== 'object') {
    testCase.appendErrorMessage('Result for id ' + id + ' is not an object; actual type: ' + typeof(instance));
  }
  if (instance === null) {
    testCase.appendErrorMessage('Result for id ' + id + ' is null.');
    return;
  }
  if (domainObject) {
    if (typeof(instance.getId) !== 'function') {
      testCase.appendErrorMessage('Result for id ' + id + ' is not a domain object');
      return;
    }
  }
  udebug.log_detail('instance for id ', id, ':', instance);
  testCase.errorIfNotEqual('fail to verify id', id, instance.where);
  testCase.errorIfNotEqual('fail to verify ttinyint', id, instance.execute);
  testCase.errorIfNotEqual('fail to verify tsmallint', id, instance.count);
  testCase.errorIfNotEqual('fail to verify tmediumint', id, instance.param);
  testCase.errorIfNotEqual('fail to verify tint', id, instance.field);
  testCase.errorIfNotEqual('fail to verify tbigint of type ' + typeof(instance.tbigint), id + '', instance.tbigint);
};

global.fail_verify_integraltypes_keyword_array = function(err, instances, ids, testCase, domainObject) {
  var i;
  if (err) {
    testCase.appendErrorMessage(err);
  } else if (instances.length !== ids.length) {
    testCase.appendErrorMessage(
        'Mismatch in length of result; expected: ' + ids.length + '; actual: ' + instances.length);
  } else {
    for (i = 0; i < ids.length; ++i) {
      // verify each instance in the result array
      verify_integraltypes_keyword(err, instances[i], ids[i], testCase, domainObject);
    }
  }
  // fail if any failures
  testCase.failOnError();
};

/***** Query by id named 'where' using qint.where.eq ***/
var t1 = new harness.ConcurrentTest("testQueryByConstructorAndPrimaryKeyQintWhere");
t1.run = function() {
  var testCase = this;
  var key = 1;
  fail_openSession(testCase, function(session) {
    mapIntegraltypesKeyword();
    // query by id a.k.a. 'where'
    session.createQuery(IntegraltypesKeywordId, function(err, qint) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var p_id = qint.param('p_id');
      qint.where(
        qint.where.eq(p_id))
        .execute({p_id: key}, fail_verify_integraltypes_keyword_array, [key], testCase, true);
    });
  });
};

/***** Query by id named 'where' using qint.field.where.eq ***/
var t2 = new harness.ConcurrentTest("testQueryByConstructorAndPrimaryKeyQintFieldWhere");
t2.run = function() {
  var testCase = this;
  var key = 2;
  fail_openSession(testCase, function(session) {
    mapIntegraltypesKeyword();
    // query by id a.k.a. 'where'
    session.createQuery(IntegraltypesKeywordId, function(err, qint) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var p_id = qint.param('p_id');
      qint.where(
        qint.field.where.eq(p_id))
        .execute({p_id: key}, fail_verify_integraltypes_keyword_array, [key], testCase, true);
    });
  });
};


module.exports.tests = [t1, t2];

