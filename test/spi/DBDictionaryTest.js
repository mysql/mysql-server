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

/*global path, fs, assert,
         driver_dir, suites_dir, adapter_dir, build_dir,
         spi_module, api_module, udebug_module,
         harness, mynode, udebug, debug,
         adapter, test_conn_properties,
         module, exports
*/

try {
  require("./suite_config.js");
} catch(e) {} 

var spi = require(spi_module);

var t1 = new harness.ConcurrentSubTest("getDataDictionary");
var t2 = new harness.ConcurrentSubTest("listTables");
var t3 = new harness.ConcurrentTest("getTable");

t3.run = function() {  
  var provider = spi.getDBServiceProvider(global.adapter),
      properties = provider.getDefaultConnectionProperties(), 
      x_conn = null,
      x_session = null,
      connect_cb, test1_cb, test2_cb;
    
  this.teardown = function() {
    // FIXME
    // session.close() does not exist in the spi?
    // if(x_session !== null) x_session.close(); 

    // WARNING -- "Deleting Ndb_cluster_connection with Ndb-object not deleted"
    if(x_conn !== null) {
      x_conn.closeSync();
    }
  };

  connect_cb = function(err, connection) {
    x_conn = connection; // for teardown

    test1_cb = function(err, sess) {
      x_session = sess;   // for teardown
      var dict = sess.getDataDictionary();
      if(typeof dict !== 'object') {
        t1.fail(); t2.fail(); return;
      }
      t1.pass();     // succesfully got a DBDictionary object
  
      test2_cb = function(error, table_list) {
        t2.errorIfNotEqual("Error return", error, undefined);
        t2.errorIfNotEqual("Bad table count", table_list.length, 2);
        t2.failOnError();
        
        dict.getTable("test","tbl1", function(err, tab) {
          console.dir(tab);
          t3.pass();
        });
      };
      dict.listTables("test", test2_cb);

    };
    // fixme:  what is the "index" argument supposed to do?
    connection.getDBSession(0, test1_cb);
  };

  provider.connect(properties, connect_cb);
}

exports.tests = [ t1, t2, t3];