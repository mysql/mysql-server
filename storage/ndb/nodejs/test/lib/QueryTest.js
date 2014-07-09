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

"use strict";

var udebug = unified_debug.getLogger("lib/QueryTest.js");
var mysql = require("mysql");

/** This is a generic query test. It runs a number of queries and reports results.
 * 
 * To use this as the run method in a test case, use linkage:
 * var temporaltypes = function temporaltypes() {};
 * new mynode.TableMapping('temporaltypes').applyToClass(temporaltypes);
 * var queryTests = [q1, q2];
 * var testQueries = new QueryTest("testTimestampQueries", temporaltypes, queryTests);
 * module.exports.tests = [testQueries];
 */

function QueryTest(name, mappings, queryTests) {
  harness.SerialTest.call(this, name);
  this.mappings = mappings;
  this.queryTests = queryTests;
}

QueryTest.prototype = new harness.ConcurrentTest();

QueryTest.prototype.run = function() {
  var testCase = this;
  var from = testCase.mappings;

  function onOpenSession(session) {
    var i = 0, j = 0, completedTestCount = 0, testCount = testCase.queryTests.length;

    function onExecute(err, results, queryTest) {
      if (queryTest.expectedError) {
        testCase.errorIfNotError(queryTest.name + ' Expected error: ' + queryTest.expectedError, err);
      } else {
        testCase.errorIfError(err);
        testCase.errorIfNull('NoResults: ' + queryTest.name, results);
      }
      if(results) {
        // check results
        // get the result ids in an array
        for (j = 0; j < results.length; ++j) {
          queryTest.resultIds[j] = results[j].id;
        }
        if (queryTest.expected.length !== results.length) {
          testCase.errorIfNotEqual(queryTest.name + ' wrong results: expected ' + 
              queryTest.expected + '; actual: ' + queryTest.resultIds,
              queryTest.expected.length, results.length);
        } else {
          if (!queryTest.ordered) {
            // compare results without considering order
            queryTest.resultIds.sort(function(a,b){return a-b;});
          }
          // compare the results one by one, in order
          for (j = 0; j < queryTest.expected.length; ++j) {
            udebug.log_detail('QueryPrimaryIndexScan.testQueries ' + queryTest.name + ' expected: '
                + queryTest.expected[j] + ' actual: ' + queryTest.resultIds[j]);
            testCase.errorIfNotEqual(queryTest.name + ' wrong result at position ' + j,
                queryTest.expected[j], queryTest.resultIds[j]);
          }
        }
      }
      if (++completedTestCount === testCount) {
        testCase.failOnError();
      }
    }

    function doQueryTest(queryTest) {
      // set up to check results
      queryTest.resultIds = [];
      // each query test gets its own query domain type

      session.createQuery(from, function(err, q) {
        if (err) {
          testCase.appendErrorMessage('QueryPrimaryIndexScan.testQueries ' + 
                                       queryTest.name + ' returned error: ' + err);
          --testCount;
          return;
        }
        q.where(queryTest.predicate(q));
        testCase.errorIfNotEqual('Wrong query type for ' + queryTest.name,
            queryTest.queryType, q.mynode_query_domain_type.queryType);
        q.execute(queryTest, onExecute, queryTest);
      });
    }
    
    testCase.queryTests.forEach(doQueryTest);
  }
  fail_openSession(testCase, onOpenSession);
};

module.exports = QueryTest;
