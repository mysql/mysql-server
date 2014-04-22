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

var errorMessages = '';


var t1 = new harness.ConcurrentTest('ProjectionTest');

var expectedCustomer = new lib.Customer(100, 'Craig', 'Walton');
var expectedShoppingCart = new lib.ShoppingCart(1000);
var expectedLineItems = [lib.createLineItem(0, 1, 10000),
                        lib.createLineItem(1, 5, 10014),
                        lib.createLineItem(2, 2, 10011),
                       ];
expectedCustomer.shoppingCart = expectedShoppingCart;

var customerProjection = {
  constructor: lib.Customer, fields: ['id', 'firstName', 'lastName'],
  relationships:  {
    'shoppingCart': {
      constructor: lib.ShoppingCart, fields: ['id'],
      relationships: {
        'lineitems': {
        constructor: lib.LineItem, fields: ['line', 'quantity', 'itemid'],
        relationships: {
          'item': {
            constructor: lib.Item, fields: ['id', 'description']
            }
          }
        }
      }
    }
  }
};

t1.run = function() {
  var testCase = this;
  var session;
  lib.mapShop();
  testCase.mappings = lib.shopDomainObjects;
  fail_openSession(testCase, function(s) {
    session = s;
    // set projection for customer
    session.useProjection(customerProjection).
    then(function() {
      return session.find(lib.Customer, 100);}).
    then(function(actualCustomer) {
      lib.verifyProjection(testCase, customerProjection, expectedCustomer, actualCustomer);
      testCase.failOnError();}, function(err) {testCase.fail(err);});
    });
};


exports.tests = [t1];
