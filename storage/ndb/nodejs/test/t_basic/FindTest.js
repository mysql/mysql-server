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
/*global t_basic, verify_t_basic, t_basic_key, fail_verify_t_basic */


/***** Find with domain object and primitive primary key ***/
var t1 = new harness.ConcurrentTest("testFindDomainObjectPrimitive");
t1.run = function() {
  var testCase = this;
  // use the domain object and primitive to find an instance
  var from = global.t_basic;
  var key = 1;
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, key, testCase, true);
  });
};

/***** Find with domain object and javascript primary key literal ***/
var t2 = new harness.ConcurrentTest("testFindDomainObjectLiteral");
t2.run = function() {
  var testCase = this;
  // use the domain object and literal to find an instance
  var from = global.t_basic;
  var key = {'id' : 2};
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, 2, testCase, true);
  });
};

/***** Find with domain object and javascript primary key object ***/
var t3 = new harness.ConcurrentTest("testFindDomainObjectObject");
t3.run = function() {
  var testCase = this;
  // use the domain object and key object to find an instance
  var from = global.t_basic;
  var key = new t_basic_key(3);
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, 3, testCase, true);
  });
};

/***** Find with table name and primitive primary key ***/
var t4 = new harness.ConcurrentTest("testFindTableNamePrimitive");
t4.run = function() {
  var testCase = this;
  var from = 't_basic';
  var key = 4;
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, 4, testCase, false);
  });
};

/***** Find with table name and javascript primary key literal ***/
var t5 = new harness.ConcurrentTest("testFindTableNameLiteral");
t5.run = function() {
  var testCase = this;
  // use table name and literal to find an instance
  var from = 't_basic';
  var key = {'id' : 5};
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, 5, testCase, false);
  });
};

/***** Find with table name and javascript primary key object ***/
var t6 = new harness.ConcurrentTest("testFindTableNameObject");
t6.run = function() {
  var testCase = this;
  var from = 't_basic';
  var key = new t_basic_key(6);
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, 6, testCase, false);
  });
};

/***** Find with domain object and javascript unique key literal ***/
var t7 = new harness.ConcurrentTest("testFindDomainObjectUniqueKeyLiteral");
t7.run = function() {
  var testCase = this;
  // use the domain object and literal to find an instance
  var from = global.t_basic;
  var key = {'magic' : 7};
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, 7, testCase, true);
  });
};

/***** Find with domain object and javascript unique key object ***/
var t8 = new harness.ConcurrentTest("testFindDomainObjectUniqueKeyObject");
t8.run = function() {
  var testCase = this;
  // use the domain object and literal to find an instance
  var from = global.t_basic;
  var key = new global.t_basic_magic_key(8);
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, 8, testCase, true);
  });
};

/***** Find with table name and javascript unique key literal ***/
var t9 = new harness.ConcurrentTest("testFindTableNameUniqueKeyLiteral");
t9.run = function() {
  var testCase = this;
  // use the table name and literal to find an instance
  var from = 't_basic';
  var key = {'magic' : 9};
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, 9, testCase, false);
  });
};

/***** Find with table name and javascript unique key object ***/
var t0 = new harness.ConcurrentTest("testFindTableNameUniqueKeyObject");
t0.run = function() {
  var testCase = this;
  // use the table name and object to find an instance
  var from = 't_basic';
  var key = new global.t_basic_magic_key(0);
  fail_openSession(testCase, function(session) {
    // key and testCase are passed to fail_verify_t_basic as extra parameters
    session.find(from, key, fail_verify_t_basic, 0, testCase, false);
  });
};

module.exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8, t9, t0];

