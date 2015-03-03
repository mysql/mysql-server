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
var udebug = unified_debug.getLogger("composition/lib.js");
var util = require("util");

function Customer(id, first, last) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.firstName = first;
    this.lastName = last;
  }
}

function ShoppingCart(id, customerid) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.customerid = customerid;
  }
}

function LineItem(line, shoppingcartid, quantity, itemid) {
  if (typeof line !== 'undefined') {
    this.line = line;
    this.shoppingcartid = shoppingcartid;
    this.quantity = quantity;
    this.itemid = itemid;
  }  
}

function Item(id, description) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.description = description;
  }  
}

function Discount(id, description, percent) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.description = description;
    this.percent = percent;
  }  
}

function mapCustomer() {
  // map customer
  var customerMapping = new mynode.TableMapping('customer');
  customerMapping.mapField('id');
  customerMapping.mapField('firstName', 'firstname');
  customerMapping.mapField('lastName', 'lastname');
  customerMapping.mapOneToOne( { 
    fieldName:  'shoppingCart', 
    target:      ShoppingCart, 
    targetField: 'customer' 
  } ); 
  customerMapping.mapManyToMany( {
    fieldName:   'discounts',
    target:      Discount,
    targetField: 'customers'
  } );

  customerMapping.applyToClass(Customer);
}

function mapShoppingCart() {
  // map shopping cart
  var shoppingCartMapping = new mynode.TableMapping('shoppingcart');
  shoppingCartMapping.mapField('id');

  shoppingCartMapping.mapOneToOne( { 
    fieldName:  'customer', 
    foreignKey: 'fkcustomerid', 
    target:     Customer
  } );

  shoppingCartMapping.mapOneToMany( { 
    fieldName:  'lineItems', 
    targetField: 'shoppingCart', 
    target:     LineItem
  } );

  shoppingCartMapping.applyToClass(ShoppingCart);
}

function mapLineItem() {
  // map line item
  var lineItemMapping = new mynode.TableMapping('lineitem');
  lineItemMapping.mapField('line');
  lineItemMapping.mapField('quantity');
  lineItemMapping.mapField('shoppingcartid');
  lineItemMapping.mapField('itemid');
  
  lineItemMapping.mapManyToOne( {
    fieldName:  'shoppingCart',
    foreignKey: 'fkshoppingcartid',
    target:     ShoppingCart
  });

  lineItemMapping.mapManyToOne( {
    fieldName:  'item',
    foreignKey: 'fkitemid',
    target:     Item
  });

  lineItemMapping.applyToClass(LineItem);
}

function mapItem() {
  var itemMapping = new mynode.TableMapping('item');
  itemMapping.mapField('id');
  itemMapping.mapField('description');

  itemMapping.mapOneToMany( { 
    fieldName:  'lineItems',
    target:      LineItem, 
    targetField: 'item' 
  } ); 
  
  itemMapping.applyToClass(Item);
}

function mapDiscount() {
  var discountMapping = new mynode.TableMapping('discount');
  discountMapping.mapField('id');
  discountMapping.mapField('description');
  discountMapping.mapField('percent');

  discountMapping.mapManyToMany( { 
    fieldName:  'customers',
    target:      Customer,
    joinTable:  'customerdiscount',
  } ); 

  discountMapping.applyToClass(Discount);
}

function CustomerDiscount(customerid, discountid) {
  if (typeof customerid !== 'undefined') {
    this.customerid = customerid;
    this.discountid = discountid;
  }
}

function mapCustomerDiscount() {
  var customerDiscountMapping = new mynode.TableMapping('customerdiscount');
  customerDiscountMapping.mapField('customerid');
  customerDiscountMapping.mapField('discountid');
  customerDiscountMapping.applyToClass(CustomerDiscount);
}

function FkDifferentDb(id) {
  if (typeof id !== 'undefined') {
    this.id = id;
  }
}

function mapFkDifferentDb() {
  var fkDifferentDbMapping = new mynode.TableMapping('testfk.fkdifferentdb');
  fkDifferentDbMapping.mapField('id');
  fkDifferentDbMapping.applyToClass(FkDifferentDb);
}

function verifyFK(testCase, tableMetadata, fks) {
  function verify(name, expected, actual) {
    var expectedValue = expected[name];
    var actualValue = actual[name];
    if (typeof actualValue === 'undefined') {
      testCase.appendErrorMessage('\nExpected ' + name + ' was undefined');
      return;
    }
    switch(typeof expectedValue) {
    case 'string':
      if (expectedValue !== actualValue) {
        testCase.appendErrorMessage('\nMismatch on ' + name + '; expected: ' + expectedValue + '; actual: ' + actualValue);
      }
      break;
    case 'object':
      if (!Array.isArray(actualValue)) {
        testCase.appendErrorMessage('\nUnexpected not an array: ' + util.inspect(actualValue));
      } else {
        expectedValue.forEach(function(element) {
          if (actualValue.indexOf(element) == -1) {
            testCase.appendErrorMessage('\nExpected element missing from ' + name + ': ' + element + ' in ' + util.inspect(actualValue));
          }
        });
      }
      break;
    }
  }
  if (!tableMetadata.foreignKeys) {
    testCase.appendErrorMessage('\nMetadata for ' + tableMetadata.name + ' did not include foreignKeys.');
  } else {
    fks.forEach(function(fkexpected) {
      var found = false;
      tableMetadata.foreignKeys.forEach(function(fkactual) {
        if (fkexpected.name === fkactual.name) {
          found = true;
          verify('targetTable', fkexpected, fkactual);
          verify('targetDatabase', fkexpected, fkactual);
          verify('columnNames', fkexpected, fkactual);
          verify('targetColumnNames', fkexpected, fkactual);
        }
      });
      if (!found) {
        testCase.appendErrorMessage('\nNo foreign key ' + fkexpected.name + ' in table metadata for ' + tableMetadata.name);
      }
    });
  }
}

function mapShop() {
  mapCustomer();
  mapShoppingCart();
  mapLineItem();
  mapItem();
  mapDiscount();
  mapCustomerDiscount();
}

var shopDomainObjects = [Customer, ShoppingCart, LineItem, Item, Discount, CustomerDiscount];

function createLineItem(line, quantity, itemid) {
  var result = new LineItem();
  result.line = line;
  result.quantity = quantity;
  result.itemid = itemid;
  return result;
}

function sortFunction(a, b) {
  // sort based on id if it exists, or on line if it exists or throw an exception
  if (typeof(a.id) !== 'undefined' && typeof(b.id) !== 'undefined') {
    return a.id - b.id;
  }
  if (typeof(a.line) !== 'undefined' && typeof(b.line) !== 'undefined') {
    return a.line - b.line;
  }
  throw new Error('Error: can only sort objects containing properties id or line.');
}

function verifyProjection(tc, p, e, a) {
  var testCase = tc;
  var projectionVerifications;
  var projectionVerification;
  var i;
  
  function verifyOneProjection() {
    udebug.log_detail('verifyOneProjection with', projectionVerifications.length, 'waiting:\n', projectionVerifications[0]);
    while (projectionVerifications.length > 0) {
      projectionVerification = projectionVerifications.shift();
      var projection = projectionVerification[0];
      var expected = projectionVerification[1];
      var actual = projectionVerification[2];
      var domainObjectName = projection.domainObject.prototype.constructor.name;
      var expectedField;
      var actualField;
      var expectedRelationship;
      var actualRelationship;
      // check that the actual object exists and is unexpected
      if ((expected !== null && actual === null) ||
           (typeof expected !== 'undefined' && typeof actual === 'undefined')) {
        testCase.appendErrorMessage('\n' + testCase.name +
              ' VerifyProjection failure for ' + domainObjectName +
              '\nexpected: ' + util.inspect(expected) + '\nactual: ' + actual);
        continue;
      }
      // check for null and undefined 
      if ((typeof expected === 'undefined' && typeof actual === 'undefined') ||
          (expected === null && actual === null)) {
        continue;
      }
      // verify the fields first
      projection.fields.forEach(function(fieldName) {
        expectedField = expected[fieldName];
        actualField = actual[fieldName];
        if (expectedField != actualField) {
          testCase.appendErrorMessage('\n' + testCase.name +
              ' VerifyProjection failure for ' + domainObjectName + ' field ' + fieldName +
              '\nexpected: ' + expectedField + '\nactual: ' + actualField);
        }
      });
      // now verify the relationships (iteratively)
      if (projection.relationships) {
        Object.keys(projection.relationships).forEach(function(relationshipName) {
          expectedRelationship = expected[relationshipName];
          actualRelationship = actual[relationshipName];
          if (Array.isArray(expectedRelationship)) {
            if (Array.isArray(actualRelationship)) {
              // we need to sort the actual array
              // TODO let the user provide a sort function
              actualRelationship.sort(sortFunction);
              if (expectedRelationship.length === actualRelationship.length) {
                // check each value in turn
                for (i = 0; i < expectedRelationship.length; ++i) {
                  projectionVerifications.push([projection.relationships[relationshipName],
                      expectedRelationship[i], actualRelationship[i]]);
                }
              } else {
                testCase.appendErrorMessage('\n' + testCase.name +
                  ' VerifyProjection failure for ' + domainObjectName +
                  ' relationship ' + relationshipName +
                  ' expected relationship length: ' + expectedRelationship.length +
                  ' actual relationship length: ' + actualRelationship.length);
              }
            } else {
              testCase.appendErrorMessage('\n' + testCase.name +
                  ' VerifyProjection failure for ' + domainObjectName +
                  ' relationship ' + relationshipName +
                  ' actual relationship is not an array: ' + actualRelationship);
            }
          } else {
            // expected value is an object
            if ((typeof expectedRelationship === 'undefined' && typeof actualRelationship !== 'undefined') ||
                (expectedRelationship === null && actualRelationship !== null) ||
                (typeof expectedRelationship !== 'undefined' & typeof actualRelationship === 'undefined') ||
                (expectedRelationship !== null && actualRelationship === null)) {
              // error
              testCase.appendErrorMessage('\n' + testCase.name +
                  ' VerifyProjection failure for ' + domainObjectName +
                  ' relationship ' + relationshipName +
                  '\nexpected relationship: ' + util.inspect(expectedRelationship) +
                  '\nactual relationship: ' + util.inspect(actualRelationship));
            } else {
              if (!((typeof expectedRelationship === 'undefined' && typeof actualRelationship === 'undefined') ||
                    (expectedRelationship === null && actualRelationship === null))) {
                // we need to check the values
                projectionVerifications.push([projection.relationships[relationshipName],
                  expectedRelationship, actualRelationship]);
              }
            }
          }
        });
      }
      verifyOneProjection();
    }
  }
  // verifyProjection starts here
  projectionVerifications = [[p, e, a]];
  verifyOneProjection();
}


exports.Customer = Customer;
exports.mapCustomer = mapCustomer;
exports.ShoppingCart = ShoppingCart;
exports.mapShoppingCart = mapShoppingCart;
exports.LineItem = LineItem;
exports.mapLineItem = mapLineItem;
exports.Item = Item;
exports.mapItem = mapItem;
exports.Discount = Discount;
exports.mapDiscount = mapDiscount;
exports.CustomerDiscount = CustomerDiscount;
exports.mapCustomerDiscount = mapCustomerDiscount;
exports.FkDifferentDb = FkDifferentDb;
exports.mapFkDifferentDb = mapFkDifferentDb;
exports.mapShop = mapShop;
exports.verifyFK = verifyFK;
exports.verifyProjection = verifyProjection;
exports.shopDomainObjects = shopDomainObjects;
exports.createLineItem = createLineItem;
