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

var t1 = new harness.ConcurrentTest("RelationshipMappingTest");
t1.run = function() {
  var testCase = this;
  lib.mapShop();
  testCase.mappings = lib.shopDomainObjects;
  fail_openSession(testCase, function(session) {
    // get TableMetadata for all tables
    session.getTableMetadata('test', 'customer').
    then(function(customerMetadata){
      return session.getTableMetadata('test', 'shoppingcart');}).
    then(function(shoppingCartMetadata){
      lib.verifyFK(testCase, shoppingCartMetadata, 
        [{name:'fkcustomerid',targetTable:'customer',targetDatabase:'test', columnNames:['customerid'],targetColumnNames:['id']}
        ]);
      return session.getTableMetadata('test', 'lineitem');}).
    then(function(lineItemMetadata){
      lib.verifyFK(testCase, lineItemMetadata, 
          [{name:'fkitemid',targetTable:'item',targetDatabase:'test', columnNames:['itemid'],targetColumnNames:['id']},
          {name:'fkshoppingcartid',targetTable:'shoppingcart',targetDatabase:'test', columnNames:['shoppingcartid'],targetColumnNames:['id']}
          ]);
      return session.getTableMetadata('test', 'item');}).
    then(function(itemMetadata){
      return session.getTableMetadata('test', 'discount');}).
    then(function(discountMetadata){
      return session.getTableMetadata('test', 'customerdiscount');}).
    then(function(customerDiscountMetadata){
      lib.verifyFK(testCase, customerDiscountMetadata, 
          [{name:'fkcustomerid',targetTable:'customer',targetDatabase:'test', columnNames:['customerid'],targetColumnNames:['id']},
          {name:'fkdiscountid',targetTable:'discount',targetDatabase:'test', columnNames:['discountid'],targetColumnNames:['id']}
          ]);
      return session.close();}).
    then(function() {testCase.failOnError();}, function(err) {testCase.fail(err);});
  });
};


var t2 = new harness.ConcurrentTest("SecondDatabaseMappingTest");
t2.run = function() {
  var testCase = this;
  lib.mapFkDifferentDb();
  testCase.mappings = [lib.FkDifferentDb];
  fail_openSession(testCase, function(session) {
    // get TableMetadata for testfk
    session.getTableMetadata('testfk', 'fkdifferentdb').
    then(function(t1Metadata){
      lib.verifyFK(testCase, t1Metadata,
          [{name:'fkcustomerid',targetTable:'customer',targetDatabase:'test', columnNames:['id'],targetColumnNames:['id']}
          ]);
      return session.close();}).
    then(function() {testCase.failOnError();}, function(err) {testCase.fail(err);});
  });
};



exports.tests = [t1, t2];
