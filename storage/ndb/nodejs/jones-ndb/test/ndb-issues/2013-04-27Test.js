/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

/* 
CREATE TABLE if not exists `towns2` (
  `town` varchar(50) NOT NULL,
  `county` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`town`)
);
*/

var jones = require("database-jones");

var t1 = new harness.ConcurrentTest("readTownByPK");

var Town = function(name, county) {
  this.town = name;
  this.county = county;
};

// create basic object<->table mappings                                                                                                  
var annotations = new jones.TableMapping('towns2').applyToClass(Town);

//check results of find                                                                                                                  
var onFind = function(err, result) {
  if(global.test_conn_properties.use_mapped_ndb_record) {
    try {
      t1.errorIfNotEqual("Expected SQLState WCTOR", "WCTOR", err.sqlstate);
    }
    catch(e) {
      t1.appendErrorMessage("Expected DBOperation Error with SQLState WCTOR");
    }
  }
  t1.failOnError();
};

//check results of insert                                                                                                                
var onInsert = function(err, object, session) {
  session.find(Town, 'Maidenhead', onFind);
};

// insert an object                                                                                                                      
var onSession = function(err, session) {
  var data = new Town('Maidenhead', 'Berkshire');
  session.persist(data, onInsert, data, session);
};


t1.mappings = Town;
t1.run = function() {
  fail_openSession(t1, function(session, testCase) {
    onSession(null, session)
  });
};


module.exports.tests = [ t1 ];


