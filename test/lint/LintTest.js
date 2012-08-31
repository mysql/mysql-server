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

var linter;
var skipTests = false;
try {
  linter = require("jslint/lib/linter");
}
catch(e) {
  skipTests = true;
}

var lintOptions = {
  "vars"      : true,     // allow multiple var declarations
  "plusplus"  : true,     // ++ operators
  "white"     : true,     // misc. white space
  "stupid"    : true      // sync methods
};

function runLintTest() {
  if(skipTests) {
    return this.skip("jslint not avaliable");
  }

  var e, i;
  var data = fs.readFileSync(this.sourceFile, "utf8");  
  var result = linter.lint(data, lintOptions);

  if(! result.ok) { 
    for (i = 0; i < result.errors.length; i += 1) {
      e = result.errors[i];
      if (e) { this.appendErrorMessage('  Line ' + e.line + ': ' + e.reason); }
    }
  }
  return true;
}

function LintTest(sourceFile) {
  var t = new harness.SerialTest(path.basename(sourceFile));
  t.sourceFile = path.join(adapter_dir, sourceFile);
  t.run = runLintTest;
  return t;
}

exports.tests = [];
exports.tests.push(new LintTest("impl/common/DBTableHandler.js"));



