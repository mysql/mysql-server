/*
 Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights
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
/*global fail_verify_integraltypes_array */
/* TODO: replace qint.mynode_query_domain_type.queryType with appropriate EXPLAIN once EXPLAIN is implemented */

/***** Query by primary key id ***/
var t1 = new harness.ConcurrentTest("testQueryByConstructorAndPrimaryKey");
t1.run = function() {
  var testCase = this;
  // use id to find an instance
  var from = global.integraltypes;
  var key = 1;
  fail_openSession(testCase, function(session) {
    // query by id
    session.createQuery(from, function(err, qint) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var p_id = qint.param('p_id');
      qint.where(qint.id.eq(p_id));
      testCase.errorIfNotEqual('Incorrect queryType', 0, qint.mynode_query_domain_type.queryType);
      qint.execute({p_id: key}, fail_verify_integraltypes_array, [key], testCase, true);
    });
  });
};

/***** Query by unique key tint ***/
var t2 = new harness.ConcurrentTest("testQueryByConstructorAndUniqueKey");
t2.run = function() {
  var testCase = this;
  // use tint to find an instance
  var from = global.integraltypes;
  var key = 2;
  fail_openSession(testCase, function(session) {
    // query by unique key tint
    session.createQuery(from, function(err, qint) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var p_int = qint.param('p_int');
      qint.where(qint.tint.eq(p_int));
      testCase.errorIfNotEqual('Incorrect queryType', 1, qint.mynode_query_domain_type.queryType);
      qint.execute({p_int: key}, fail_verify_integraltypes_array, [key], testCase, true);
    });
  });
};

/***** Query by primary key id ***/
var t3 = new harness.ConcurrentTest("testQueryByDomainObjectAndPrimaryKey");
t3.run = function() {
  var testCase = this;
  // use id to find an instance
  var from = new global.integraltypes(0);
  var key = 3;
  fail_openSession(testCase, function(session) {
    // query by id
    session.createQuery(from, function(err, qint) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var p_id = qint.param('p_id');
      qint.where(qint.id.eq(p_id));
      testCase.errorIfNotEqual('Incorrect queryType', 0, qint.mynode_query_domain_type.queryType);
      qint.execute({p_id: key}, fail_verify_integraltypes_array, [key], testCase, true);
    });
  });
};

/***** Query by unique key tint ***/
var t4 = new harness.ConcurrentTest("testQueryByDomainObjectAndUniqueKey");
t4.run = function() {
  var testCase = this;
  // use tint to find an instance
  var from = new global.integraltypes(0);
  var key = 4;
  fail_openSession(testCase, function(session) {
    // query by unique key tint
    session.createQuery(from, function(err, qint) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var p_int = qint.param('p_int');
      qint.where(qint.tint.eq(p_int));
      testCase.errorIfNotEqual('Incorrect queryType', 1, qint.mynode_query_domain_type.queryType);
      qint.execute({p_int: key}, fail_verify_integraltypes_array, [key], testCase, true);
    });
  });
};

/***** Query by primary key id ***/
var t5 = new harness.ConcurrentTest("testQueryByTableNameAndPrimaryKey");
t5.run = function() {
  var testCase = this;
  // use id to find an instance
  var from = 'integraltypes';
  var key = 5;
  fail_openSession(testCase, function(session) {
    // query by id
    session.createQuery(from, function(err, qint) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var p_id = qint.param('p_id');
      qint.where(qint.id.eq(p_id));
      testCase.errorIfNotEqual('Incorrect queryType', 0, qint.mynode_query_domain_type.queryType);
      qint.execute({p_id: key}, fail_verify_integraltypes_array, [key], testCase, false);
    });
  });
};

/***** Query by unique key tint ***/
var t6 = new harness.ConcurrentTest("testQueryByTableNameAndUniqueKey");
t6.run = function() {
  var testCase = this;
  // use tint to find an instance
  var from = 'integraltypes';
  var key = 6;
  fail_openSession(testCase, function(session) {
    // query by unique key tint
    session.createQuery(from, function(err, qint) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var p_int = qint.param('p_int');
      qint.where(qint.tint.eq(p_int));
      testCase.errorIfNotEqual('Incorrect queryType', 1, qint.mynode_query_domain_type.queryType);
      qint.execute({p_int: key}, fail_verify_integraltypes_array, [key], testCase, false);
    });
  });
};

module.exports.tests = [t1, t2, t3, t4, t5, t6];

