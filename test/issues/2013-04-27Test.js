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

/* 
CREATE TABLE if not exists `towns` (
  `town` varchar(50) NOT NULL,
  `county` varchar(50) DEFAULT NULL,
  PRIMARY KEY (`town`)
);
*/

var nosql = require('../..');

var t1 = new harness.ConcurrentTest("readTownByPK");

var Town = function(name, county) {
//  if(name) {
    this.town = name;
    this.county = county;
//  }
};

// create basic object<->table mappings                                                                                                  
var annotations = new nosql.TableMapping('towns').applyToClass(Town);

//check results of find                                                                                                                  
var onFind = function(err, result) {
  if (err) {
    t1.appendErrorMessage(err);
  } else {
    t1.errorIfNotEqual("town",  "Maidenhead", result.town);
    t1.errorIfNotEqual("county", "Berkshire", result.county);
  }
  t1.failOnError();
};

//check results of insert                                                                                                                
var onInsert = function(err, object, session) {

  if (err) {
    t1.appendErrorMessage(err);
   } else {

    // Now read the data back out from the database                                                                                      
    session.find(Town, 'Maidenhead', onFind);
  }
};

// insert an object                                                                                                                      
var onSession = function(err, session) {
  var data = new Town('Maidenhead', 'Berkshire');
  session.persist(data, onInsert, data, session);
};

onFailOpenSession = function(session, testCase) {
  onSession(null, session);
}


t1.mappings = Town;
t1.run = function() {
  fail_openSession(t1, onFailOpenSession);
}


module.exports.tests = [ t1 ];


