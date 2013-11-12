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

var udebug = unified_debug.getLogger("integraltypes/QueryPrimaryIndexScanLimitTest.js");

var t0 = new harness.ConcurrentTest('test');
t0.run = function() {
  this.pass();
};

/** limit can be used without order */
var q1 = {name: 'q1', p1: 4, p2: 6, expected: [], limit: 0, queryType: 2, ordered: false, predicate: function(qdt) {
  return qdt.id.ge(qdt.param('p1')).and(qdt.id.le(qdt.param('p2')));
}};

/** no error; no results */
var q2 = {name: 'q2', p1: 4, p2: 6, expected: [], limit: 0, order: 'ASC', queryType: 2, ordered: false, predicate: function(qdt) {
  return qdt.id.ge(qdt.param('p1')).and(qdt.id.le(qdt.param('p2')));
}};

/** limit must be between 0 and MAX_LIMIT */
var q3 = {name: 'q3', p1: 4, p2: 6, expected: [], limit: -1, order: 'ASC', expectedError: 'Bad limit parameter', queryType: 2, ordered: false, predicate: function(qdt) {
  return qdt.id.ge(qdt.param('p1')).and(qdt.id.le(qdt.param('p2')));
}};

/** limit 2 ascending */
var q4 = {name: 'q4', p1: 4, p2: 6, expected: [4, 5], limit: 2, order: 'ASC', queryType: 2, ordered: true, predicate: function(qdt) {
  return qdt.id.ge(qdt.param('p1')).and(qdt.id.le(qdt.param('p2')));
}};

/** limit 2 descending */
var q5 = {name: 'q5', p1: 4, p2: 6, expected: [6, 5], limit: 2, order: 'DESC', queryType: 2, ordered: true, predicate: function(qdt) {
  return qdt.id.ge(qdt.param('p1')).and(qdt.id.le(qdt.param('p2')));
}};

/** skip cannot be used without order */
var q6 = {name: 'q6', p1: 4, p2: 6, expected: [], skip: 0, expectedError: 'Bad skip parameter', queryType: 2, ordered: false, predicate: function(qdt) {
  return qdt.id.ge(qdt.param('p1')).and(qdt.id.le(qdt.param('p2')));
}};

/** skip must be between 0 and MAX_SKIP */
var q7 = {name: 'q7', p1: 4, p2: 6, expected: [], skip: -1, order: 'ASC', expectedError: 'Bad skip parameter', queryType: 2, ordered: false, predicate: function(qdt) {
  return qdt.id.ge(qdt.param('p1')).and(qdt.id.le(qdt.param('p2')));
}};

/** skip 1 limit 2 */
var q8 = {name: 'q8', p1: 4, p2: 6, expected: [5, 6], skip: 1, limit: 2, order: 'ASC', queryType: 2, ordered: true, predicate: function(qdt) {
  return qdt.id.ge(qdt.param('p1')).and(qdt.id.le(qdt.param('p2')));
}};

/** illegal order */
var q9 = {name: 'q9', p1: 4, p2: 6, expected: [5, 6], skip: 1, limit: 2, order: 'ascending' , expectedError: 'Bad order parameter', queryType: 2, ordered: true, predicate: function(qdt) {
  return qdt.id.ge(qdt.param('p1')).and(qdt.id.le(qdt.param('p2')));
}};


var queryTests = [q1, q2, q3, q4, q5, q6, q7, q8, q9];

/***** Build and run queries ***/
var testQueries = new harness.ConcurrentTest("testQueries");
testQueries.run = function() {
  var testCase = this;
  var from = global.integraltypes;
  testCase.mappings = from;

  function onOpenSession(session) {
    var i = 0, j = 0, completedTestCount = 0, testCount = queryTests.length;

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
            udebug.log_detail('QueryPrimaryIndexScan.testQueries ' + queryTest.testName + ' expected: '
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
                                       queryTest.testName + ' returned error: ' + err);
          --testCount;
          return;
        }
        q.where(queryTest.predicate(q));
        testCase.errorIfNotEqual('Wrong query type for ' + queryTest.testName,
            queryTest.queryType, q.mynode_query_domain_type.queryType);
        q.execute(queryTest, onExecute, queryTest);
      });
    }
    
    queryTests.forEach(doQueryTest);
  }

  fail_openSession(testCase, onOpenSession);
};


module.exports.tests = [testQueries];

