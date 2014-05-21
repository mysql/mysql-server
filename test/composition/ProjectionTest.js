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
var udebug = unified_debug.getLogger("ProjectionTest.js");

var t1 = new harness.ConcurrentTest('t1 ProjectionTest');
var t2 = new harness.ConcurrentTest('t2 ProjectionTestDefaultNull');
var t3 = new harness.ConcurrentTest('t3 ProjectionTestDefaultEmptyArray');
var t4 = new harness.ConcurrentTest('t4 ProjectionTestAddProjectionField');
var t5 = new harness.ConcurrentTest('t5 ProjectionTestAddProjectionRelationship');

var itemProjection = new mynode.Projection(lib.Item);
itemProjection.addFields('id', 'description');
var lineItemProjection = new mynode.Projection(lib.LineItem);
lineItemProjection.addFields('line', ['quantity', 'itemid']);
lineItemProjection.addRelationship('item', itemProjection);
var shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
shoppingCartProjection.addFields('id');
shoppingCartProjection.addRelationship('lineItems', lineItemProjection);
var customerProjection = new mynode.Projection(lib.Customer);
customerProjection.addFields('id', 'firstName', 'lastName');
customerProjection.addRelationship('shoppingCart', shoppingCartProjection);


t1.run = function() {
  var testCase = this;
  var session;
  lib.mapShop();

  var expectedCustomer = new lib.Customer(100, 'Craig', 'Walton');
  var expectedShoppingCart = new lib.ShoppingCart(1000);
  var expectedLineItem0 = lib.createLineItem(0, 1, 10000);
  var expectedLineItem1 = lib.createLineItem(1, 5, 10014);
  var expectedLineItem2 = lib.createLineItem(2, 2, 10011);
  var expectedLineItems = [expectedLineItem0,
                           expectedLineItem1,
                           expectedLineItem2
                         ];
  var expectedItem10000 = new lib.Item(10000, 'toothpaste');
  var expectedItem10011 = new lib.Item(10011, 'half and half');
  var expectedItem10014 = new lib.Item(10014, 'holy bible');
  expectedLineItem0.item = expectedItem10000;
  expectedLineItem1.item = expectedItem10014;
  expectedLineItem2.item = expectedItem10011;
  expectedShoppingCart.lineItems = expectedLineItems;
  expectedCustomer.shoppingCart = expectedShoppingCart;

  fail_openSession(testCase, function(s) {
    session = s;
    // find with projection for customer
    session.find(customerProjection, '100').
    then(function(actualCustomer) {
      lib.verifyProjection(testCase, customerProjection, expectedCustomer, actualCustomer);
      testCase.failOnError();
      }, function(err) {testCase.fail(err);}).
    then(null, function(err) {
      testCase.fail(err);
    });
  });
};

/** Projection test default null mapping.
 * Customer 101 has no shopping cart. */
t2.run = function() {
  var testCase = this;
  var session;
  lib.mapShop();

  var expectedCustomer = new lib.Customer(101, 'Sam', 'Burton');
  expectedCustomer.shoppingCart = null;
  fail_openSession(testCase, function(s) {
    session = s;
    // find with projection with default null value for shoppingCart
    session.find(customerProjection, '101').
    then(function(actualCustomer) {
      lib.verifyProjection(testCase, customerProjection, expectedCustomer, actualCustomer);
      testCase.failOnError();
      }, function(err) {testCase.fail(err);}).
    then(null, function(err) {
      testCase.fail(err);
    });
  });
};

/** Projection test empty array mapping 
 * Shopping cart 1003 has no line items */
t3.run = function() {
  var testCase = this;
  var session;
  lib.mapShop();

  var expectedShoppingCart = new lib.ShoppingCart(1003);
  expectedShoppingCart.lineItems = [];
  var expectedCustomer = new lib.Customer(103, 'Burn', 'Sexton');
  expectedCustomer.shoppingCart = expectedShoppingCart;
  fail_openSession(testCase, function(s) {
    session = s;
    // find with projection with default null value for shoppingCart
    // shopping cart 1003 has no line items
    session.find(customerProjection, '103').
    then(function(actualCustomer) {
      lib.verifyProjection(testCase, customerProjection, expectedCustomer, actualCustomer);
      testCase.failOnError();
      }, function(err) {testCase.fail(err);}).
    then(null, function(err) {
      testCase.fail(err);
    });
  });
};


/** Projection test add projection field after using the projection in a find
 * Shopping cart 1003 has no line items */
t4.run = function() {
  var testCase = this;
  var session;
  lib.mapShop();

  var customerProjection = new mynode.Projection(lib.Customer);
  customerProjection.addFields('id', 'firstName');
  var shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
  shoppingCartProjection.addFields('id');
  shoppingCartProjection.addRelationship('customer', customerProjection);
  var expectedShoppingCart = new lib.ShoppingCart(1003);
  var expectedCustomer = new lib.Customer(103, 'Burn');
  var expectedCustomer2 = new lib.Customer(103, 'Burn', 'Sexton');
  expectedShoppingCart.customer = expectedCustomer;
  fail_openSession(testCase, function(s) {
    session = s;
    // find shopping cart with customer projection
    session.find(shoppingCartProjection, '1003').
    then(function(actualShoppingCart) {
      lib.verifyProjection(testCase, shoppingCartProjection, expectedShoppingCart, actualShoppingCart);
      // add last name to customer projection
      customerProjection.addFields('lastName');
      expectedShoppingCart.customer = expectedCustomer2;
      return session.find(shoppingCartProjection, 1003);
    }).
    then(function(actualShoppingCart2) {
      lib.verifyProjection(testCase, shoppingCartProjection, expectedShoppingCart, actualShoppingCart2);
      testCase.failOnError();
    }, function(err) {testCase.fail(err);}).
    then(null, function(err) {
      testCase.fail(err);
    });
  });
};


/** Projection test add projection relationship after using the projection in a find
 * Shopping cart 1003 has no line items */
t5.run = function() {
  var testCase = this;
  var session;
  lib.mapShop();

  var customerProjection = new mynode.Projection(lib.Customer);
  customerProjection.addFields('id', 'firstName');
  var shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
  shoppingCartProjection.addFields('id');
  var expectedShoppingCart = new lib.ShoppingCart(1003);
  var expectedCustomer = new lib.Customer(103, 'Burn');
  fail_openSession(testCase, function(s) {
    session = s;
    // find shopping cart with customer projection
    session.find(shoppingCartProjection, '1003').
    then(function(actualShoppingCart) {
      lib.verifyProjection(testCase, shoppingCartProjection, expectedShoppingCart, actualShoppingCart);
      // add customer projection to shopping cart projection
      shoppingCartProjection.addRelationship('customer', customerProjection);
      expectedShoppingCart.customer = expectedCustomer;
      return session.find(shoppingCartProjection, 1003);
    }).
    then(function(actualShoppingCart2) {
      lib.verifyProjection(testCase, shoppingCartProjection, expectedShoppingCart, actualShoppingCart2);
      testCase.failOnError();
    }, function(err) {testCase.fail(err);}).
    then(null, function(err) {
      testCase.fail(err);
    });
  });
};


exports.tests = [t1, t2, t3, t4, t5];
