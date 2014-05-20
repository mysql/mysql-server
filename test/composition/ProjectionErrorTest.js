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

var lib = require('./lib.js');

/** Error conditions to be tested:
 * t1 projection field that does not exist in the mapping
 * t2 relationship field that does not exist in the mapping
 * t3 projected field that is a relationship in the mapping
 * t4 projected relationship field that is a non-relationship field in the mapping
 * t5 relationship field that would cause a recursion
 * t6 projection domain object that is not mapped
 */
var t1 = new harness.ConcurrentTest('t1 ProjectionFieldNotMapped');
var t2 = new harness.ConcurrentTest('t2 ProjectionRelationshipNotMapped');
var t3 = new harness.ConcurrentTest('t3 ProjectionFieldIsRelationship');
var t4 = new harness.ConcurrentTest('t4 ProjectionRelationshipIsField');
var t5 = new harness.ConcurrentTest('t5 ProjectionRecursion');
var t6 = new harness.ConcurrentTest('t6 ProjectionUnmappedDomainObject');


t1.run = function() {
  var testCase = this;
  var session;
  var expectedErrorMessage = 'unmappedField is not mapped';
  var customerProjection = new mynode.Projection(lib.Customer);
  customerProjection.addFields('unmappedField');
  lib.mapShop();

  fail_openSession(testCase, function(s) {
    session = s;
    session.find(customerProjection, '100').
    then(function(actualCustomer) {
      testCase.fail('t1 Unexpected success of find with error projection.');
    }, function(err) {
      if (err.message.indexOf(expectedErrorMessage) === -1) {
        testCase.fail('t1 Wrong error message; does not include ' + expectedErrorMessage + ' in ' + err.message);
      } else {
        testCase.pass();
      }
    });
  });
};

t2.run = function() {
  var testCase = this;
  var session;
  var expectedErrorMessage = 'unmappedRelationship is not mapped';
  var customerProjection = new mynode.Projection(lib.Customer);
  var shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
  customerProjection.addRelationship('unmappedRelationship', shoppingCartProjection);
  lib.mapShop();

  fail_openSession(testCase, function(s) {
    session = s;
    session.find(customerProjection, '100').
    then(function(actualCustomer) {
      testCase.fail('t2 Unexpected success of find with error projection.');
    }, function(err) {
      if (err.message.indexOf(expectedErrorMessage) === -1) {
        testCase.fail('t2 Wrong error message; does not include ' + expectedErrorMessage + ' in ' + err.message);
      } else {
        testCase.pass();
      }
    });
  });
};

t3.run = function() {
  var testCase = this;
  var session;
  var expectedErrorMessage = 'shoppingCart must not be a relationship';
  var customerProjection = new mynode.Projection(lib.Customer);
  var shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
  customerProjection.addField('shoppingCart');
  lib.mapShop();

  fail_openSession(testCase, function(s) {
    session = s;
    session.find(customerProjection, '100').
    then(function(actualCustomer) {
      testCase.fail('t3 Unexpected success of find with error projection.');
    }, function(err) {
      if (err.message.indexOf(expectedErrorMessage) === -1) {
        testCase.fail('t3 Wrong error message; does not include ' + expectedErrorMessage + ' in ' + err.message);
      } else {
        testCase.pass();
      }
    });
  });
};

t4.run = function() {
  var testCase = this;
  var session;
  var expectedErrorMessage = 'shoppingCart must not be a relationship';
  var customerProjection = new mynode.Projection(lib.Customer);
  var shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
  customerProjection.addField('shoppingCart');
  lib.mapShop();

  fail_openSession(testCase, function(s) {
    session = s;
    session.find(customerProjection, '100').
    then(function(actualCustomer) {
      testCase.fail('t4 Unexpected success of find with error projection.');
    }, function(err) {
      if (err.message.indexOf(expectedErrorMessage) === -1) {
        testCase.fail('t4 Wrong error message; does not include ' + expectedErrorMessage + ' in ' + err.message);
      } else {
        testCase.pass();
      }
    });
  });
};

t5.run = function() {
  var testCase = this;
  var session;
  var expectedErrorMessage = 'Recursive projection for Customer';
  var customerProjection = new mynode.Projection(lib.Customer);
  var shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
  customerProjection.addRelationship('shoppingCart', shoppingCartProjection);
  shoppingCartProjection.addRelationship('customer', customerProjection);
  lib.mapShop();

  fail_openSession(testCase, function(s) {
    session = s;
    session.find(customerProjection, '100').
    then(function(actualCustomer) {
      testCase.fail('t5 Unexpected success of find with error projection.');
    }, function(err) {
      if (err.message.indexOf(expectedErrorMessage) === -1) {
        testCase.fail('t5 Wrong error message; does not include ' + expectedErrorMessage + ' in ' + err.message);
      } else {
        testCase.pass();
      }
    });
  });
};

t6.run = function() {
  var testCase = this;
  var session;
  var expectedErrorMessage = 'constructor for Unmapped';
  function Unmapped() {}
  var unmappedProjection = new mynode.Projection(Unmapped);
  lib.mapShop();

  fail_openSession(testCase, function(s) {
    session = s;
    session.find(unmappedProjection, '100').
    then(function(actualCustomer) {
      testCase.fail('t6 Unexpected success of find with error projection.');
    }, function(err) {
      if (err.message.indexOf(expectedErrorMessage) === -1) {
        testCase.fail('t6 Wrong error message; does not include ' + expectedErrorMessage + ' in ' + err.message);
      } else {
        testCase.pass();
      }
    });
  });
};


exports.tests = [t1, t2, t3, t4, t5, t6];
