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

/*global fs,util,harness,path,adapter_dir,suites_dir,spi_doc_dir */

"use strict";

var linter, skipTests;

try      { skipTests = false; linter = require("jslint/lib/linter"); }
catch(e) { skipTests = true; }

var jslintOptions = {
  "vars"      : true,     // allow multiple var declarations
  "plusplus"  : true,     // ++ operators
  "white"     : true,     // misc. white space
  "stupid"    : true,     // sync methods
  "node"      : true,     // node.js globals
  "nomen"     : true,     // allow dangling underscore
};


function lintTest(basePath, sourceFile) {
  var t = new harness.SerialTest(path.basename(sourceFile));
  t.sourceFile = path.join(basePath, sourceFile);

  t.run = function runLintTest() {
    if(skipTests) { return this.skip("jslint not avaliable"); }

    var e, i, n=0;
    var data = fs.readFileSync(this.sourceFile, "utf8");  
    var result = linter.lint(data, jslintOptions);

    if(! result.ok) {
      console.log("Errors for "+ this.name +":");
      for (i = 0; i < result.errors.length; i += 1) {
        e = result.errors[i];
        if(e) {
          n += 1;
          console.log(' * Line %d[%d]: %s', e.line, e.character, e.reason);
        }
      }    
      this.appendErrorMessage(util.format("%d jslint error%s", n, n===1 ? '':'s'));
    }
    return true;
  };
  
  return t;
}

exports.tests = [];

function checkSource(file) {
  exports.tests.push(lintTest(adapter_dir, file));
}

function checkTest(file) {
  exports.tests.push(lintTest(suites_dir, file));
}

function checkSpiDoc(file) {
  exports.tests.push(lintTest(spi_doc_dir, file));
}

// ****** SOURCES FILES TO CHECK ********** //

checkSource("impl/common/DBTableHandler.js");
checkSource("impl/common/UserContext.js");

checkSource("impl/mysql/mysql_service_provider.js");
checkSource("impl/mysql/MySQLConnectionPool.js");
checkSource("impl/mysql/MySQLConnection.js");
//checkSource("impl/mysql/MySQLDictionary.js");

checkSource("impl/ndb/ndb_service_provider.js");
checkSource("impl/ndb/NdbConnectionPool.js");
checkSource("impl/ndb/NdbSession.js");
checkSource("impl/ndb/NdbOperation.js");
checkSource("impl/ndb/NdbTransactionHandler.js");
checkSource("impl/ndb/NdbTypeEncoders.js");

checkSpiDoc("DBOperation");

checkSource("api/unified_debug.js");
checkSource("api/SessionFactory.js");
checkSource("api/Session.js");
checkSource("api/Annotations.js");
checkSource("api/FieldMapping.js");
checkSource("api/TableMapping.js");
// checkSource("api/mynode.js");

// ****** TEST FILES TO CHECK ********** //
checkTest("lint/LintTest.js");

checkTest("Driver.js");
checkTest("harness.js");
checkTest("spi/SmokeTest.js");
checkTest("spi/DBServiceProviderTest.js");
checkTest("spi/DBConnectionPoolTest.js");
checkTest("spi/DBDictionaryTest.js");
checkTest("spi/InsertAndDeleteIntTest.js");
checkTest("spi/ClearSmokeTest.js");
checkTest("spi/BasicVarcharTest.js");

