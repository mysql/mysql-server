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

/** This is the smoke test for the t_basic suite.
 */

require("./lib.js");

var test = new harness.SmokeTest("SmokeTest");

test.run = function() {
  var t = this;
  harness.SQL.create(this.suite, function(error) {
    if (error) {
      t.fail('createSQL failed: ' + error);
    } 
    var props = new mynode.ConnectionProperties(global.adapter);
    var annotations = new mynode.Annotations();
    annotations.strict(true);
    annotations.mapClass(global.t_basic.prototype, {
      "table" : "t_basic",
      "schema" : "def",
      "database" : "test",
      "autoIncrementBatchSize" : 1,
      "fields" : [
                  {"name": "id", "column" : "id"},
                  {"name": "age", "column" : "age"},
                  {"name": "name", "column" : "name"},
                  {"name": "magic", "column" : "magic"}
      ]
      }
    );
    mynode.connect(props, annotations, function(err, factory) {
      if(err) {
        t.fail(err);
      } else {
        factory.close();
        t.pass();
      }
    });
  });
};

module.exports.tests = [test];
