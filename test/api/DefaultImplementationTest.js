/*
 Copyright (c) 2013, Oracle and/or its affiliates. All rights
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

var t1 = new harness.ConcurrentTest("testDefaultImplementation");
t1.run = function() {
  mynode.connect(global.adapter, null, function(err, sessionFactory) {
    if (sessionFactory) {
      t1.errorIfNotEqual('implementation name mismatch', global.adapter, sessionFactory.properties.implementation);
      t1.failOnError();
      sessionFactory.close();
    } else {
      t1.fail('could not obtain sessionFactory using adapter name ' + global.adapter + ' in mysql.connect');
    }
  });
};

module.exports.tests = [t1];
