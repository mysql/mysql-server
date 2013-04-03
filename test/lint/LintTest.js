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

var skipTests = false;
var linter, lintName;

try      { lintName = "jslint"; linter = require("jslint/lib/linter").lint;  }
catch(e) { try       { lintName = "jshint"; linter = require("jshint").JSHINT; }
           catch(f)  { skipTests = true;  }
         }

var jslintOptions = {
  "vars"      : true,     // allow multiple var declarations
  "plusplus"  : true,     // ++ operators
  "white"     : true,     // misc. white space
  "stupid"    : true,     // sync methods
  "node"      : true,     // node.js globals
  "nomen"     : true,     // allow dangling underscore
};

var jshintOptions = {
  "node"      : true,     // node.js globals
  "latedef"   : true      // check for variables used before defined
};

var lintOptions = lintName === "jslint" ? jslintOptions : jshintOptions;

function lintTest(basePath, sourceFile, ignoreLines) {
  var t = new harness.SerialTest(path.basename(sourceFile));
  var ignore;
  t.sourceFile = path.join(basePath, sourceFile);

  t.run = function runLintTest() {
    if(skipTests) { return this.skip("jslint not avaliable"); }

    var e, i, n=0, line;
    var data = fs.readFileSync(this.sourceFile, "utf8");  
    var result = linter(data, lintOptions);
    var ok, errors, msg = "";

    /* Adapt to differing APIs of jslint and jshint */
    if(typeof result === 'boolean') {
      /* We are using jshint */
      ok = result;
      errors = linter.errors;
    }
    else {
      ok = result.ok;
      errors = result.errors;
    }

    if(! ok) {
      for (i = 0; i < errors.length; i += 1) {
        e = errors[i];
        if (e) {
          var ignoreLine = ignoreLines.indexOf(e.line);
          if (ignoreLine === -1) {
            n += 1;
            msg += util.format('\n * Line %d[%d]: %s', e.line, e.character, e.reason);
            ignoreLines.indexOf(e.line);        
          } else {
            var ignored = ignoreLines.splice(ignoreLine, 1);
          }
        }
      }
        if (n > 0) {
          msg = util.format("%d %s error%s", n, lintName, n===1 ? '':'s') + msg;
          this.appendErrorMessage(msg);
        }
      }
      // if any errors left, that is also an error
      if (ignoreLines.length !== 0) {
        for (i = 0; i < ignoreLines.length; ++i) {
          msg = "Ignored error line " + ignoreLines[i] + " did not contain an error";
          this.appendErrorMessage(msg);
        }
      }
    return true;
  };
  
  return t;
}

function checkSource(file) {
  var ignoreLines = Array.prototype.slice.call(arguments);
  ignoreLines.shift(); // remove the file name
  exports.tests.push(lintTest(adapter_dir, file, ignoreLines));
}

function checkTest(file) {
  var ignoreLines = Array.prototype.slice.call(arguments);
  ignoreLines.shift(); // remove the file name
  exports.tests.push(lintTest(suites_dir, file, ignoreLines));
}

function checkSpiDoc(file) {
  var ignoreLines = Array.prototype.slice.call(arguments);
  ignoreLines.shift(); // remove the file name
  exports.tests.push(lintTest(spi_doc_dir, file, ignoreLines));
}

/// ******** SMOKE TEST FOR THIS SUITE ******* ///
exports.tests = [];

var smokeTest = new harness.SmokeTest("jslint smoke test");
smokeTest.run = function runLintSmokeTest() {
  if(skipTests) {
    this.fail("jslint is not available");
  }
  else {
    this.pass();
  }
};
exports.tests.push(smokeTest);


// ****** SOURCES FILES TO CHECK ********** //

checkSource("impl/common/DBTableHandler.js");
//checkSource("impl/common/UserContext.js");

checkSource("impl/mysql/mysql_service_provider.js");
checkSource("impl/mysql/MySQLConnectionPool.js");
checkSource("impl/mysql/MySQLConnection.js");
checkSource("impl/mysql/MySQLDictionary.js",
    168); // Line 166[7]: Missing 'break' after 'case'

checkSource("impl/ndb/ndb_service_provider.js");
checkSource("impl/ndb/NdbConnection.js");
checkSource("impl/ndb/NdbConnectionPool.js",
   249 // Line 294[15]: Expected a conditional expression and instead saw an assignment.
);
checkSource("impl/ndb/NdbSession.js");
checkSource("impl/ndb/NdbOperation.js",
  327 //Line 294[12]: Expected a conditional expression and instead saw an assignment.
);
checkSource("impl/ndb/NdbTransactionHandler.js");

checkSpiDoc("DBOperation");

checkSource("api/unified_debug.js");
checkSource("api/SessionFactory.js");
checkSource("api/Query.js");
checkSource("api/Session.js");
checkSource("api/TableMapping.js",
 121 // Line 121[3]: The body of a for in should be wrapped ...
);
checkSource("api/mynode.js",
 57 // Line 57[5]: The body of a for in should be wrapped ...
);

// ****** TEST FILES TO CHECK ********** //
checkTest("lint/LintTest.js");

checkTest("driver.js");
checkTest("lib/harness.js");
checkTest("lib/test_properties.js");
checkTest("spi/SmokeTest.js");
checkTest("spi/DBServiceProviderTest.js");
checkTest("spi/DBConnectionPoolTest.js");
checkTest("spi/DBDictionaryTest.js");
checkTest("spi/InsertAndDeleteIntTest.js");
checkTest("spi/ClearSmokeTest.js");
checkTest("spi/BasicVarcharTest.js");
