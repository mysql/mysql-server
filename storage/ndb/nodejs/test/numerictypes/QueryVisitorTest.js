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
var udebug = unified_debug.getLogger("integraltypes/QueryVisitorTest.js");

var t0 = new harness.ConcurrentTest('test');
t0.run = function() {
  this.pass();
};

var q1 = {
  'predicate': function(qint) {
    return qint.tint.eq(qint.param('pint'));
  },
  'sqlText': 'tint = ?',
  'param': 'pint'
};

var q2 = {
  'predicate': function(qint) {
    return qint.tint.ne(qint.param('pint'));
  },
  'sqlText': 'tint != ?',
  'param': 'pint'
};

var q3 = {
  'predicate': function(qint) {
    return qint.tint.lt(qint.param('pint'));
  },
  'sqlText': 'tint < ?',
  'param': 'pint'
};

var q4 = {
  'predicate': function(qint) {
    return qint.tint.le(qint.param('pint'));
  },
  'sqlText': 'tint <= ?',
  'param': 'pint'
};

var q5 = {
  'predicate': function(qint) {
    return qint.tint.gt(qint.param('pint'));
  },
  'sqlText': 'tint > ?',
  'param': 'pint'
};

var q6 = {
  'predicate': function(qint) {
    return qint.tint.ge(qint.param('pint'));
  },
  'sqlText': 'tint >= ?',
  'param': 'pint'
};

var q7 = {
  'predicate': function(qint) {
    return qint.tint.eq(qint.param('pint')).not();
  },
  'sqlText': ' NOT (tint = ?)',
  'param': 'pint'
};

var q8 = {
  'predicate': function(qint) {
    return qint.tint.between(qint.param('p1'), qint.param('p2'));
  },
  'sqlText': 'tint BETWEEN ? AND ?',
  'param': 'p1,p2'
};

var q9 = {
  'predicate': function(qint) {
    return qint.tint.isNull();
  },
  'sqlText': 'tint IS NULL',
  'param': ''
};

var q10 = {
  'predicate': function(qint) {
    return qint.tint.isNotNull();
  },
  'sqlText': 'tint IS NOT NULL',
  'param': ''
};

var q11 = {
  'predicate': function(qint) {
    return qint.tint.ge(qint.param('pint_lower')).and(qint.tint.lt(qint.param('pint_upper')));
  },
  'sqlText': '(tint >= ?) AND (tint < ?)',
  'param': 'pint_lower,pint_upper'
};

var q12 = {
  'predicate': function(qint) {
    return qint.tint.eq(qint.param('pint_first')).or(qint.tint.eq(qint.param('pint_second')));
   },
  'sqlText': '(tint = ?) OR (tint = ?)',
  'param': 'pint_first,pint_second'
};

var q13 = {
  'predicate': function(qint) {
    return qint.tint.eq(qint.param('pint_first'))
      .or(qint.tint.eq(qint.param('pint_second')))
      .or(qint.tint.eq(qint.param('pint_third')))
      .or(qint.tint.eq(qint.param('pint_fourth')));
    },
  'sqlText': '(tint = ?) OR (tint = ?) OR (tint = ?) OR (tint = ?)',
  'param': 'pint_first,pint_second,pint_third,pint_fourth'
};

var q14 = {
    'predicate': function(qint) {
      return qint.tint.eq(qint.param('pint_first'))
        .and(qint.tint.eq(qint.param('pint_second')))
        .and(qint.tint.eq(qint.param('pint_third')))
        .and(qint.tint.eq(qint.param('pint_fourth')));
      },
    'sqlText': '(tint = ?) AND (tint = ?) AND (tint = ?) AND (tint = ?)',
    'param': 'pint_first,pint_second,pint_third,pint_fourth'
  };

var q15 = {
    'predicate': function(qint) {
      return qint.tint.eq(qint.param('pint_first'))
        .and(qint.tint.eq(qint.param('pint_second')))
        .and(qint.tint.eq(qint.param('pint_third')))
        .or(qint.tint.eq(qint.param('pint_fourth')));
      },
    'sqlText': '((tint = ?) AND (tint = ?) AND (tint = ?)) OR (tint = ?)',
    'param': 'pint_first,pint_second,pint_third,pint_fourth'
  };

var q16 = {
    'predicate': function(qint) {
      return qint.tint.eq(qint.param('pint_first'))
        .or(qint.tint.eq(qint.param('pint_second')))
        .or(qint.tint.eq(qint.param('pint_third')))
        .and(qint.tint.eq(qint.param('pint_fourth')));
      },
    'sqlText': '((tint = ?) OR (tint = ?) OR (tint = ?)) AND (tint = ?)',
    'param': 'pint_first,pint_second,pint_third,pint_fourth'
  };

var q17 = {
    'predicate': function(qint) {
      return qint.not(qint.tint.eq(qint.param('pint_first')).or(qint.tint.eq(qint.param('pint_second'))));
     },
    'sqlText': ' NOT ((tint = ?) OR (tint = ?))',
    'param': 'pint_first,pint_second'
  };

var q18 = {
    'predicate': function(qint) {
      return qint.tint.eq(qint.param('pint_first')).orNot((qint.tint.eq(qint.param('pint_second'))));
     },
    'sqlText': '(tint = ?) OR ( NOT (tint = ?))',
    'param': 'pint_first,pint_second'
  };


var q19 = {
    'predicate': function(qint) {
      return qint.tint.eq(qint.param('pint_first')).andNot(qint.tint.eq(qint.param('pint_second')));
     },
    'sqlText': '(tint = ?) AND ( NOT (tint = ?))',
    'param': 'pint_first,pint_second'
  };

var queryTests = [q1, q2, q3, q4, q5, q6, q7, q8, q9, q10, q11, q12, q13, q14, q15, q16, q17, q18, q19];

/***** Verify that predicates are constructed properly ***/
var t1 = new harness.ConcurrentTest("testQueryVisitor");
t1.run = function() {
  var testCase = this;
  // use id to find an instance
  var from = global.integraltypes;
  testCase.mappings = from;
  fail_openSession(testCase, function(session) {
    // query by id
    session.createQuery(from, function(err, qint) {
      if (err) {
        testCase.fail(err);
        return;
      }
      var q = 0;
      // create the query predicate
      queryTests.forEach(function(queryTest) {
        ++q;
        var predicate = queryTest.predicate(qint);
        var sql = predicate.getSQL();
        var formalParameters = sql.formalParameters;
        var sqlText = sql.sqlText;
        var parameterNames = '';
        var separator = '';
        formalParameters.forEach(function(formalParameter) {
          parameterNames += separator + formalParameter.name;
          separator = ',';
        });
        testCase.errorIfNotEqual('q' + q + ' sqlText mismatch', queryTest.sqlText, sqlText);
        testCase.errorIfNotEqual('q' + q + ' parameter mismatch', queryTest.param, parameterNames);
      });
    });
    testCase.failOnError();
  });
};


module.exports.tests = [t1];

