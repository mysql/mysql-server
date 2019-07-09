/*
 Copyright (c) 2014, Oracle and/or its affiliates. All rights
 reserved.
 
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
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 02110-1301  USA
 */

// Create a TableMapping from a literal
// Open a session
// Attempt an operation on the table

var nosql = require('../..');

var t1 = new harness.ConcurrentTest("useLiteralMapping");

function RowConstructor() {
};

// Literal Mapping
var mapping, literalMapping;

literalMapping = {
  database      : "test",
  table         : "towns",
  mapAllColumns : true
};

mapping = new nosql.TableMapping(literalMapping);

mapping.applyToClass(RowConstructor);

t1.run = function() {
  fail_openSession(t1, function(session, testCase) {
    session.persist(RowConstructor, { town: 'Glen Rock', county: 'Bergen'}, function(err) {
      t1.errorIfError(err);
      t1.failOnError();
    });
  });
}

module.exports.tests = [ t1 ];


