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

var t1 = new harness.ConcurrentTest("RelationshipMappingTest");
var t2 = new harness.ConcurrentTest("SecondDatabaseMappingTest");
var errorMessages = '';

function Customer(id, first, last) {
  if (typeof id !== 'undefined') {
    this.id = id;
    this.firstName = firstname;
    this.lastName = lastname;
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

function verifyFK(tableMetadata, fks) {
  function verify(name, expected, actual) {
    var expectedValue = expected[name];
    var actualValue = actual[name];
    if (typeof actualValue === 'undefined') {
      errorMessages += '\nExpected ' + name + ' was undefined';
      return;
    }
    switch(typeof expectedValue) {
    case 'string':
      if (expectedValue !== actualValue) {
        errorMessages += '\nMismatch on ' + name + '; expected: ' + expectedValue + '; actual: ' + actualValue;
      }
      break;
    case 'object':
      if (!Array.isArray(actualValue)) {
        errorMessages += '\nUnexpected not an array: ' + util.inspect(actualValue);
      } else {
        expectedValue.forEach(function(element) {
          if (actualValue.indexOf(element) == -1) {
            errorMessages += '\nExpected element missing from ' + name + ': ' + element + ' in ' + util.inspect(actualValue);
          }
        });
      }
      break;
    }
  }
  if (!tableMetadata.foreignKeys) {
    errorMessages += '\nMetadata for ' + tableMetadata.name + ' did not include foreignKeys.';
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
        errorMessages += '\nNo foreign key ' + fkexpected.name + ' in table metadata for ' + tableMetadata.name;
      }
    });
  }
}

t1.run = function() {
  var testCase = this;
  function fail(err) {
    errorMessages += err.message;
    testCase.fail(errorMessages);
  }
  mapShoppingCart();
  mapCustomer();
  mapLineItem();
  mapItem();
  mapDiscount();
  mapCustomerDiscount();
  testCase.mappings = [ShoppingCart, Customer, LineItem, Item, Discount, CustomerDiscount];
  fail_openSession(testCase, function(session) {
    // get TableMetadata for all tables
    session.getTableMetadata('test', 'customer').
    then(function(customerMetadata){
      return session.getTableMetadata('test', 'shoppingcart');}).
    then(function(shoppingCartMetadata){
      verifyFK(shoppingCartMetadata, 
        [{name:'fkcustomerid',targetTable:'customer',targetDatabase:'test', columnNames:['customerid'],targetColumnNames:['id']}
        ]);
      return session.getTableMetadata('test', 'lineitem');}).
    then(function(lineItemMetadata){
      verifyFK(lineItemMetadata, 
          [{name:'fkitemid',targetTable:'item',targetDatabase:'test', columnNames:['itemid'],targetColumnNames:['id']},
          {name:'fkshoppingcartid',targetTable:'shoppingcart',targetDatabase:'test', columnNames:['shoppingcartid'],targetColumnNames:['id']}
          ]);
      return session.getTableMetadata('test', 'item');}).
    then(function(itemMetadata){
      return session.getTableMetadata('test', 'discount');}).
    then(function(discountMetadata){
      return session.getTableMetadata('test', 'customerdiscount');}).
    then(function(customerDiscountMetadata){
      verifyFK(customerDiscountMetadata, 
          [{name:'fkcustomerid',targetTable:'customer',targetDatabase:'test', columnNames:['customerid'],targetColumnNames:['id']},
          {name:'fkdiscountid',targetTable:'discount',targetDatabase:'test', columnNames:['discountid'],targetColumnNames:['id']}
          ]);
      if (errorMessages !== '') {
        testCase.fail(errorMessages);
      } else {
        testCase.pass();
      }
      }, function(err) {fail(err);});
  });
};


t2.run = function() {
  var testCase = this;
  function fail(err) {
    console.log('RelationshipMapping.t2', JSON.stringify(err));
    errorMessages += err.message;
    testCase.fail(errorMessages);
  }
  function failOnError() {
    testCase.failOnError();
  }
  mapFkDifferentDb();
  testCase.mappings = [FkDifferentDb];
  fail_openSession(testCase, function(session) {
    // get TableMetadata for all tables
    session.getTableMetadata('testfk', 'fkdifferentdb').
    then(function(t1Metadata){
      verifyFK(t1Metadata,
          [{name:'fkcustomerid',targetTable:'customer',targetDatabase:'test', columnNames:['id'],targetColumnNames:['id']}
          ]);
      failOnError();
      }, function(err) {fail(err);});
  });
};


exports.tests = [t1, t2];
