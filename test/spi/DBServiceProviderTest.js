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

// TODO:  DBServiceProviderTest needs to run before DBConnectionTest.

var impl = "ndb";
var spi = require(spi_module);
var service = spi.getDBServiceProvider(impl);

var t1 = new harness.SerialTest("getDefaultConnectionProperties");
t1.run = function() {
  var properties = service.getDefaultConnectionProperties();
  return true; // test is complete
};


var t2 = new harness.SerialTest("getFactoryKey");
t2.run = function() {
  var properties = service.getDefaultConnectionProperties();
  var key = service.getFactoryKey(properties);
  return true; // test is complete
}


var t3 = new harness.SerialTest("connectSync");
t3.run = function() {
  var properties = service.getDefaultConnectionProperties();
  var connection = service.connectSync(properties);
  connection.closeSync();
  return true; // test is complete
}

exports.tests = [t1, t2, t3];
