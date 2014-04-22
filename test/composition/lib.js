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

function Customer(id, first, last) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.firstName = first;
    this.lastName = last;
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

  customerMapping.applyToClass(Customer);
}

function ShoppingCart(id, customerid) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.customerid = customerid;
  }
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
    fieldName:  'lineitems', 
    targetField: 'shoppingCart', 
    target:     LineItem
  } );

  shoppingCartMapping.applyToClass(ShoppingCart);
}

function LineItem(line, shoppingcartid, quantity, itemid) {
  if (typeof line !== 'undefined') {
    this.line = line;
    this.shoppingcartid = shoppingcartid;
    this.quantity = quantity;
    this.itemid = itemid;
  }  
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
    foreignKey: 'fkshoppingcart',
    target:     ShoppingCart
  });

  lineItemMapping.mapManyToOne( {
    fieldName:  'item',
    foreignKey: 'fkitemid',
    target:     Item
  });

  lineItemMapping.applyToClass(LineItem);
}

function Item(id, description) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.description = description;
  }  
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

function Discount(id, description, percent) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.description = description;
    this.percent = percent;
  }  
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
    targetField: 'discounts' 
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

function verifyProjection(testCase, projection, expected, actual) {
  var domainObjectName = projection.constructor.prototype.constructor.name;
  var expectedField;
  var actualField;
  var expectedRelationship;
  var actualRelationship;
  var index;
  // verify the fields first
  projection.fields.forEach(function(fieldName) {
    expectedField = expected[fieldName];
    actualField = actual[fieldName];
    if (Array.isArray(expectedField)) {
      // expected value is an array; check each value in turn
      for (index = 0; index < expectedField.length; ++index) {
        if (expectedField[index] != actualField[index]) {
          testCase.appendErrorMessage('verifyProjection failure for ' + domainObjectName + ' field ' + fieldName +
              ' at index ' + index +
              ' expected: ' + expectedField + '; actual: ' + actualField);
        }
      }
    } else {
      if (expectedField != actualField) {
        testCase.appendErrorMessage('verifyProjection failure for ' + domainObjectName + ' field ' + fieldName +
          ' expected: ' + expectedField + '; actual: ' + actualField);
      }
    }
  });
  // now verify the relationships (recursively)
  // this doesn't quite work yet because of many-valued relationships
  if (projection.relationships) {
    Object.keys(projection.relationships).forEach(function(relationshipName) {
    expectedRelationship = expected[relationshipName];
    actualRelationship = actual[relationshipName];
    if (actualRelationship) {
      console.log('verifyProjection for', domainObjectName, 'relationship', relationshipName, util.inspect(expectedRelationship));
      verifyProjection(testCase, projection.relationships[relationshipName],
        expectedRelationship, actualRelationship);
    } else {
      testCase.appendErrorMessage('verifyProjection failure for ' + domainObjectName + ' relationship ' + relationshipName +
          ' actual relationship was not present.');
    }
  });
  }
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
