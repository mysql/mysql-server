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
var t6 = new harness.ConcurrentTest('t6 ProjectionTestManyToMany');
var t7 = new harness.ConcurrentTest('t7 ProjectionTestManyToManyOtherSide');
var t8 = new harness.ConcurrentTest('t8 ProjectionTestManyToManyNoResults');

var itemProjection = new mynode.Projection(lib.Item);
itemProjection.addFields('id', 'description');
itemProjection.name = 'itemProjection';
var lineItemProjection = new mynode.Projection(lib.LineItem);
lineItemProjection.addFields('line', ['quantity', 'itemid']);
lineItemProjection.addRelationship('item', itemProjection);
lineItemProjection.name = 'lineItemProjection';
var shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
shoppingCartProjection.addFields('id');
shoppingCartProjection.addRelationship('lineItems', lineItemProjection);
shoppingCartProjection.name = 'shoppingCartProjection';
var customerProjection = new mynode.Projection(lib.Customer);
customerProjection.addFields('id', 'firstName', 'lastName');
customerProjection.addRelationship('shoppingCart', shoppingCartProjection);
customerProjection.name = 'customerProjection';
var discountProjection = new mynode.Projection(lib.Discount);
discountProjection.addField('id', 'description');
discountProjection.addRelationship('customers', customerProjection);
discountProjection.name = 'discountProjection';

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

  var t4customerProjection = new mynode.Projection(lib.Customer);
  t4customerProjection.addFields('id', 'firstName');
  t4customerProjection.name = 't4customerProjection';
  var t4shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
  t4shoppingCartProjection.addFields('id');
  t4shoppingCartProjection.addRelationship('customer', t4customerProjection);
  t4shoppingCartProjection.name = 't4shoppingCartProjection';
  var expectedShoppingCart = new lib.ShoppingCart(1003);
  var expectedCustomer = new lib.Customer(103, 'Burn');
  var expectedCustomer2 = new lib.Customer(103, 'Burn', 'Sexton');
  expectedShoppingCart.customer = expectedCustomer;
  fail_openSession(testCase, function(s) {
    session = s;
    // find shopping cart with customer projection
    session.find(t4shoppingCartProjection, '1003').
    then(function(actualShoppingCart) {
      lib.verifyProjection(testCase, t4shoppingCartProjection, expectedShoppingCart, actualShoppingCart);
      // add last name to customer projection
      t4customerProjection.addFields('lastName');
      expectedShoppingCart.customer = expectedCustomer2;
      return session.find(t4shoppingCartProjection, 1003);
    }).
    then(function(actualShoppingCart2) {
      lib.verifyProjection(testCase, t4shoppingCartProjection, expectedShoppingCart, actualShoppingCart2);
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
  customerProjection.name = 't5customerProjection';
  var shoppingCartProjection = new mynode.Projection(lib.ShoppingCart);
  shoppingCartProjection.addFields('id');
  shoppingCartProjection.name = 't5shoppingCartProjection';
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

/** Projection test many to many with join table defined on "left side".
 * Shopping cart 1003 has no line items.
 * Customer 101 has no shopping cart. */
t6.run = function() {
  var testCase = this;
  var session;
  lib.mapShop();

  var expectedShoppingCart1003 = new lib.ShoppingCart(1003);
  expectedShoppingCart1003.lineItems = [];
  var expectedCustomer103 = new lib.Customer(103, 'Burn', 'Sexton');
  var expectedCustomer101 = new lib.Customer(101, 'Sam', 'Burton');
  expectedCustomer103.shoppingCart = expectedShoppingCart1003;
  expectedCustomer101.shoppingCart = null;
  var expectedDiscount = new lib.Discount(3, 'internet special');
  expectedDiscount.customers = [expectedCustomer101, expectedCustomer103];
  fail_openSession(testCase, function(s) {
    session = s;
    // customer 103 has shopping cart 1003 which has no line items
    // customer 101 has no shopping cart
    session.find(discountProjection, '3').
    then(function(actualDiscount) {
      lib.verifyProjection(testCase, discountProjection, expectedDiscount, actualDiscount);
      testCase.failOnError();
      }, function(err) {testCase.fail(err);}).
    then(null, function(err) {
      testCase.fail(err);
    });
  });
};

/** Projection test many to many with join table defined on "other side"
 */
t7.run = function() {
  var testCase = this;
  var session;
  lib.mapShop();

  var t7discountProjection = new mynode.Projection(lib.Discount);
  t7discountProjection.addFields('description');
  t7discountProjection.name = 't7discountProjection';
  var t7customerProjection = new mynode.Projection(lib.Customer);
  t7customerProjection.addFields('id', 'firstName', 'lastName');
  t7customerProjection.addRelationship('discounts', t7discountProjection);
  t7customerProjection.name = 't7customerProjection';
  var expectedCustomer101 = new lib.Customer(101, 'Sam', 'Burton');
  var expectedDiscount1 = new lib.Discount(1, 'good customer');
  var expectedDiscount3 = new lib.Discount(3, 'internet special');
  var expectedDiscount4 = new lib.Discount(4, 'closeout');
  expectedCustomer101.discounts = [expectedDiscount1, expectedDiscount3, expectedDiscount4];
  fail_openSession(testCase, function(s) {
    session = s;
    // customer 101 has three discounts
    session.find(t7customerProjection, '101').
    then(function(actualCustomer) {
      lib.verifyProjection(testCase, t7customerProjection, expectedCustomer101, actualCustomer);
      testCase.failOnError();
      }, function(err) {testCase.fail(err);}).
    then(null, function(err) {
      testCase.fail(err);
    });
  });
};

/** Projection test row from primary projection does not exist when mapping many to many
 * Discount code 10 does not exist */
t8.run = function() {
  var testCase = this;
  var session;
  lib.mapShop();

  var t8expectedDiscount = null;
  fail_openSession(testCase, function(s) {
    session = s;
    // discount code 10 does not exist
    session.find(discountProjection, 10).
    then(function(actualDiscount) {
      lib.verifyProjection(testCase, customerProjection, t8expectedDiscount, actualDiscount);
      testCase.failOnError();
      }, function(err) {testCase.fail(err);}).
    then(null, function(err) {
      testCase.fail(err);
    });
  });
};



exports.tests = [t1, t2, t3, t4, t5, t6, t7, t8];
