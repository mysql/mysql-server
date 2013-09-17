/*
 Copyright (c) 2012, 2013, Oracle and/or its affiliates. All rights
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

/*global unified_debug, util */

"use strict";

var     udebug     = unified_debug.getLogger("Query.js");
var userContext    = require('../impl/common/UserContext.js');

var keywords = ['param', 'where', 'field', 'execute'];

var QueryParameter;
var QueryHandler;
var QueryEq, QueryNe, QueryLe, QueryLt, QueryGe, QueryGt, QueryBetween, QueryIn, QueryIsNull, QueryIsNotNull;
var QueryNot, QueryAnd, QueryOr;

/** QueryDomainType function param */
var param = function(name) {
  return new QueryParameter(this, name);
};

/** QueryDomainType function where */
var where = function(predicate) {
  var mynode = this.mynode_query_domain_type;
  mynode.predicate = predicate;
  mynode.queryHandler = new QueryHandler(mynode.dbTableHandler, predicate);
  mynode.queryType = mynode.queryHandler.queryType;
  this.prototype = {};
  return this;
};

/** QueryDomainType function execute */
var execute = function() {
  var session = this.mynode_query_domain_type.session;
  var context = new userContext.UserContext(arguments, 2, 2, session, session.sessionFactory);
  // delegate to context's execute for execution
  context.executeQuery(this);
};

var queryDomainTypeFunctions = {};
queryDomainTypeFunctions.where = where;
queryDomainTypeFunctions.param = param;
queryDomainTypeFunctions.execute = execute;

/**
 * QueryField represents a mapped field in a domain object. QueryField is used to build
 * QueryPredicates by comparing the field to parameters.
 * @param queryDomainType
 * @param field
 * @return
 */
var QueryField = function(queryDomainType, field) {
  udebug.log_detail('QueryField<ctor>', field.fieldName);
//  this.class = 'QueryField'; // useful for debugging
//  this.fieldName = field.fieldName; // useful for debugging
  this.queryDomainType = queryDomainType;
  this.field = field;
};

QueryField.prototype.eq = function(queryParameter) {
  return new QueryEq(this, queryParameter);
};

QueryField.prototype.le = function(queryParameter) {
  return new QueryLe(this, queryParameter);
};

QueryField.prototype.ge = function(queryParameter) {
  return new QueryGe(this, queryParameter);
};

QueryField.prototype.lt = function(queryParameter) {
  return new QueryLt(this, queryParameter);
};

QueryField.prototype.gt = function(queryParameter) {
  return new QueryGt(this, queryParameter);
};

QueryField.prototype.ne = function(queryParameter) {
  return new QueryNe(this, queryParameter);
};

QueryField.prototype.between = function(queryParameter1, queryParameter2) {
  return new QueryBetween(this, queryParameter1, queryParameter2);
};

QueryField.prototype.in = function(queryParameter) {
  return new QueryIn(this, queryParameter);
};

QueryField.prototype.isNull = function() {
  return new QueryIsNull(this);
};

QueryField.prototype.isNotNull = function() {
  return new QueryIsNotNull(this);
};

QueryField.prototype.toString = function() {
  udebug.log_detail('QueryField.toString: ', this.field.fieldName);
  return this.field.fieldName;
};

/** Query Domain Type represents a domain object that can be used to create and execute queries.
 * It encapsulates the dbTableHandler (obtained from the domain object or table name),
 * the session (required to execute the query), and the filter which limits the result.
 * @param session the user Session
 * @param dbTableHandler the dbTableHandler
 * @param domainObject true if the query results are domain objects
 */
var QueryDomainType = function(session, dbTableHandler, domainObject) {
  udebug.log('QueryDomainType<ctor>', dbTableHandler.dbTable.name);
  // avoid most name conflicts: put all implementation artifacts into the property mynode_query_domain_type
  this.mynode_query_domain_type = {};
  this.field = {};
  var mynode = this.mynode_query_domain_type;
  mynode.session = session;
  mynode.dbTableHandler = dbTableHandler;
  mynode.domainObject = domainObject;
  var queryDomainType = this;
  // initialize the functions (may be overridden below if a field has the name of a keyword)
  queryDomainType.where = where;
  queryDomainType.param = param;
  queryDomainType.execute = execute;
  
  var fieldName, queryField;
  // add a property for each field in the table mapping
  mynode.dbTableHandler.fieldNumberToFieldMap.forEach(function(field) {
    fieldName = field.fieldName;
    queryField = new QueryField(queryDomainType, field);
    udebug.log_detail('QueryDomainType<ctor> queryField for', fieldName, ':', queryField);
    if (keywords.indexOf(fieldName) === -1) {
      // field name is not a keyword
      queryDomainType[fieldName] = queryField;
    } else {
      udebug.log_detail('QueryDomainType<ctor> field', fieldName, 'is a keyword.');
      // field name is a keyword
      // allow e.g. qdt.where.id
      if (fieldName !== 'field') {
        // if field is a reserved word but not a function, skip setting the function
        queryDomainType[fieldName] = queryDomainTypeFunctions[fieldName];
        queryDomainType[fieldName].eq = QueryField.prototype.eq;
        queryDomainType[fieldName].field = queryField.field;
      }
      // allow e.g. qdt.field.where
      queryDomainType.field[fieldName] = queryField;
    }
  });
};

QueryDomainType.prototype.not = function(queryPredicate) {
  return new QueryNot(queryPredicate);
};

/**
 * QueryParameter represents a named parameter for a query. The QueryParameter marker is used
 * as the comparand for QueryField.
 * @param queryDomainType
 * @param name
 * @return
 */
QueryParameter = function(queryDomainType, name) {
  udebug.log_detail('QueryParameter<ctor>', name);
  this.queryDomainType = queryDomainType;
  this.name = name;
};

QueryParameter.prototype.toString = function() {
  return '?' + this.name;
};

/******************************************************************************
 *                 SQL VISITOR
 *****************************************************************************/
var SQLVisitor = function(rootPredicateNode) {
  this.rootPredicateNode = rootPredicateNode;
  rootPredicateNode.sql = {};
  rootPredicateNode.sql.formalParameters = [];
  rootPredicateNode.sql.sqlText = 'initialized';
  this.parameterIndex = 0;
};

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
SQLVisitor.prototype.visitQueryComparator = function(node) {
  // set up the sql text in the node
  var columnName = node.queryField.field.fieldName;
  node.sql.sqlText = columnName + node.comparator + '?';
  // assign ordered list of parameters to the top node
  this.rootPredicateNode.sql.formalParameters[this.parameterIndex++] = node.parameter;
};

/** Handle nodes QueryAnd, QueryOr */
SQLVisitor.prototype.visitQueryNaryPredicate = function(node) {
  var i;
  // all n-ary predicates have at least two
  node.predicates[0].visit(this); // sets up the sql.sqlText in the node
  node.sql.sqlText = '(' + node.predicates[0].sql.sqlText + ')';
  for (i = 1; i < node.predicates.length; ++i) {
    node.sql.sqlText += node.operator;
    node.predicates[i].visit(this);
    node.sql.sqlText += '(' + node.predicates[i].sql.sqlText + ')';
  }
};

/** Handle nodes QueryNot */
SQLVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  node.predicates[0].visit(this); // sets up the sql.sqlText in the node
  node.sql.sqlText = node.operator + '(' + node.predicates[0].sql.sqlText + ')';
};

/** Handle nodes QueryIsNull, QueryIsNotNull */
SQLVisitor.prototype.visitQueryUnaryOperator = function(node) {
  var columnName = node.queryField.field.fieldName;
  node.sql.sqlText = columnName + node.operator;
};

/** Handle node QueryBetween */
SQLVisitor.prototype.visitQueryBetweenOperator = function(node) {
  var columnName = node.queryField.field.fieldName;
  node.sql.sqlText = columnName + ' BETWEEN ? AND ?';
  this.rootPredicateNode.sql.formalParameters[this.parameterIndex++] = node.formalParameters[0];
  this.rootPredicateNode.sql.formalParameters[this.parameterIndex++] = node.formalParameters[1];
};

/******************************************************************************
 *                 TOP LEVEL ABSTRACT QUERY PREDICATE
 *****************************************************************************/
var AbstractQueryPredicate = function() {
  this.sql = {};
};

AbstractQueryPredicate.prototype.markCandidateIndex = function(candidateIndex) {
  var topLevelPredicates = this.getTopLevelPredicates();
  topLevelPredicates.forEach(function(predicate) {
    predicate.mark(candidateIndex);
  }); 
};

AbstractQueryPredicate.prototype.mark = function(candidateIndex) {
};

AbstractQueryPredicate.prototype.and = function(predicate) {
  // TODO validate parameter
  return new QueryAnd(this, predicate);
};

AbstractQueryPredicate.prototype.andNot = function(predicate) {
  // TODO validate parameter
  return new QueryAnd(this, new QueryNot(predicate));
};

AbstractQueryPredicate.prototype.or = function(predicate) {
  // TODO validate parameter for OR
  return new QueryOr(this, predicate);
};

AbstractQueryPredicate.prototype.orNot = function(predicate) {
  // TODO validate parameter
  return new QueryOr(this, new QueryNot(predicate));
};

AbstractQueryPredicate.prototype.not = function() {
  // TODO validate parameter
  return new QueryNot(this);
};

AbstractQueryPredicate.prototype.getTopLevelPredicates = function() {
  return [this];
};

AbstractQueryPredicate.prototype.getSQL = function() {
  var visitor = new SQLVisitor(this);
  this.visit(visitor);
  return this.sql;
};

/******************************************************************************
 *                 ABSTRACT QUERY N-ARY PREDICATE
 *                          AND and OR
 *****************************************************************************/
var AbstractQueryNaryPredicate = function() {
};

AbstractQueryNaryPredicate.prototype = new AbstractQueryPredicate();

AbstractQueryNaryPredicate.prototype.getTopLevelPredicates = function() {
  return this.predicates;
};

AbstractQueryNaryPredicate.prototype.visit = function(visitor) {
  if (typeof(visitor.visitQueryNaryPredicate) === 'function') {
    visitor.visitQueryNaryPredicate(this);
  }
};

/******************************************************************************
 *                 ABSTRACT QUERY UNARY PREDICATE
 *                           NOT
 *****************************************************************************/
var AbstractQueryUnaryPredicate = function() {
};

AbstractQueryUnaryPredicate.prototype = new AbstractQueryPredicate();

AbstractQueryUnaryPredicate.prototype.visit = function(visitor) {
  if (typeof(visitor.visitQueryUnaryPredicate) === 'function') {
    visitor.visitQueryUnaryPredicate(this);
  }
};

/******************************************************************************
 *                 ABSTRACT QUERY COMPARATOR
 *                  eq, ne, gt, lt, ge, le
 *****************************************************************************/
var AbstractQueryComparator = function() {
};

/** AbstractQueryComparator inherits AbstractQueryPredicate */
AbstractQueryComparator.prototype = new AbstractQueryPredicate();

AbstractQueryComparator.prototype.toString = function() {
  return this.queryField.toString() + this.comparator + this.parameter.toString();
};

AbstractQueryComparator.prototype.visit = function(visitor) {
  if (typeof(visitor.visitQueryComparator) === 'function') {
    visitor.visitQueryComparator(this);
  }
};

/******************************************************************************
 *                 QUERY EQUAL
 *****************************************************************************/
QueryEq = function(queryField, parameter) {
  this.comparator = ' = ';
  this.operationCode = 4;
  this.queryField = queryField;
  this.parameter = parameter;
};

QueryEq.prototype = new AbstractQueryComparator();

QueryEq.prototype.mark = function(candidateIndex) {
  var columnNumber = this.queryField.field.columnNumber;
  var parameterName = this.parameter.name;
  udebug.log_detail('QueryEq.mark with columnNumber:', columnNumber, 'parameterName:', parameterName);
  candidateIndex.markEq(columnNumber, parameterName);
};

/******************************************************************************
 *                 QUERY LESS THAN OR EQUAL
 *****************************************************************************/
QueryLe = function(queryField, parameter) {
  this.comparator = ' <= ';
  this.operationCode = 0;
  this.queryField = queryField;
  this.parameter = parameter;
};

QueryLe.prototype = new AbstractQueryComparator();

QueryLe.prototype.mark = function(candidateIndex) {
  var columnNumber = this.queryField.field.columnNumber;
  var parameterName = this.parameter.name;
  udebug.log_detail('QueryLe.mark with columnNumber:', columnNumber, 'parameterName:', parameterName);
  candidateIndex.markLe(columnNumber, parameterName);
};

/******************************************************************************
 *                 QUERY GREATER THAN OR EQUAL
 *****************************************************************************/
QueryGe = function(queryField, parameter) {
  this.comparator = ' >= ';
  this.operationCode = 2;
  this.queryField = queryField;
  this.parameter = parameter;
};

QueryGe.prototype = new AbstractQueryComparator();

QueryGe.prototype.mark = function(candidateIndex) {
  var columnNumber = this.queryField.field.columnNumber;
  var parameterName = this.parameter.name;
  udebug.log_detail('QueryGe.mark with columnNumber:', columnNumber, 'parameterName:', parameterName);
  candidateIndex.markGe(columnNumber, parameterName);
};

/******************************************************************************
 *                 QUERY LESS THAN
 *****************************************************************************/
QueryLt = function(queryField, parameter) {
  this.comparator = ' < ';
  this.operationCode = 1;
  this.queryField = queryField;
  this.parameter = parameter;
};

QueryLt.prototype = new AbstractQueryComparator();

QueryLt.prototype.mark = function(candidateIndex) {
  var columnNumber = this.queryField.field.columnNumber;
  var parameterName = this.parameter.name;
  udebug.log_detail('QueryGe.mark with columnNumber:', columnNumber, 'parameterName:', parameterName);
  candidateIndex.markLt(columnNumber, parameterName);
};

/******************************************************************************
 *                 QUERY GREATER THAN
 *****************************************************************************/
QueryGt = function(queryField, parameter) {
  this.comparator = ' > ';
  this.operationCode = 3;
  this.queryField = queryField;
  this.parameter = parameter;
};

QueryGt.prototype = new AbstractQueryComparator();

QueryGt.prototype.mark = function(candidateIndex) {
  var columnNumber = this.queryField.field.columnNumber;
  var parameterName = this.parameter.name;
  udebug.log_detail('QueryGt.mark with columnNumber:', columnNumber, 'parameterName:', parameterName);
  candidateIndex.markGt(columnNumber, parameterName);
};

/******************************************************************************
 *                 QUERY BETWEEN
 *****************************************************************************/
QueryBetween = function(queryField, parameter1, parameter2) {
  this.comparator = ' BETWEEN ';
  this.queryField = queryField;
  this.formalParameters = [];
  this.formalParameters[0] = parameter1;
  this.formalParameters[1] = parameter2;
};

QueryBetween.prototype = new AbstractQueryComparator();

QueryBetween.prototype.mark = function(candidateIndex) {
  // TODO this needs work to keep two parameters and one column
  var columnNumber = this.queryField.field.columnNumber;
  var parameterName = this.parameter.name;
  udebug.log_detail('QueryBetween.mark with columnNumber:', columnNumber, 'parameterName:', parameterName);
  candidateIndex.markGt(columnNumber, parameterName);
};

QueryBetween.prototype.visit = function(visitor) {
  if (typeof(visitor.visitQueryBetweenOperator) === 'function') {
    visitor.visitQueryBetweenOperator(this);
  }
};

/******************************************************************************
 *                 QUERY NOT EQUAL
 *****************************************************************************/
QueryNe = function(queryField, parameter) {
  this.comparator = ' != ';
  this.operationCode = 5;
  this.queryField = queryField;
  this.parameter = parameter;
};

QueryNe.prototype = new AbstractQueryComparator();

/******************************************************************************
 *                 QUERY IN
 *****************************************************************************/
QueryIn = function(queryField, parameter) {
  this.comparator = ' IN ';
  this.queryField = queryField;
  this.parameter = parameter;
};

QueryIn.prototype = new AbstractQueryComparator();

/******************************************************************************
 *                 ABSTRACT QUERY UNARY OPERATOR
 *****************************************************************************/
var AbstractQueryUnaryOperator = function() {
};

AbstractQueryUnaryOperator.prototype = new AbstractQueryPredicate();

AbstractQueryUnaryOperator.prototype.toString = function() {
  return this.queryField.toString() + this.comparator + this.parameter.toString();
};

AbstractQueryUnaryOperator.prototype.visit = function(visitor) {
  if (typeof(visitor.visitQueryUnaryOperator) === 'function') {
    visitor.visitQueryUnaryOperator(this);
  }
};

/******************************************************************************
 *                 QUERY IS NULL
 *****************************************************************************/
QueryIsNull = function(queryField) {
  this.operator = ' IS NULL';
  this.queryField = queryField;
};

QueryIsNull.prototype = new AbstractQueryUnaryOperator();

/******************************************************************************
 *                 QUERY IS NOT NULL
 *****************************************************************************/
QueryIsNotNull = function(queryField) {
  this.operator = ' IS NOT NULL';
  this.queryField = queryField;
};

QueryIsNotNull.prototype = new AbstractQueryUnaryOperator();

/******************************************************************************
 *                 QUERY AND
 *****************************************************************************/
QueryAnd = function(left, right) {
  this.operator = ' AND ';
  this.operationCode = 1;
  this.predicates = [left, right];
  udebug.log_detail('QueryAnd<ctor>', this);
};

QueryAnd.prototype = new AbstractQueryNaryPredicate();

QueryAnd.prototype.getTopLevelPredicates = function() {
  return this.predicates;
};

/** Override the "and" function to collect all predicates in one variable. */
QueryAnd.prototype.and = function(predicate) {
  this.predicates.push(predicate);
  return this;
};

/******************************************************************************
 *                 QUERY OR
 *****************************************************************************/
QueryOr = function(left, right) {
  this.operator = ' OR ';
  this.operationCode = 2;
  this.predicates = [left, right];
  udebug.log_detail('QueryOr<ctor>', this);
};

QueryOr.prototype = new AbstractQueryNaryPredicate();

QueryOr.prototype.getTopLevelPredicates = function() {
  return [];
};

/** Override the "or" function to collect all predicates in one variable. */
QueryOr.prototype.or = function(predicate) {
  this.predicates.push(predicate);
  return this;
};

/******************************************************************************
 *                 QUERY NOT
 *****************************************************************************/
QueryNot = function(left) {
  this.operator = ' NOT ';
  this.operationCode = 3;
  this.predicates = [left];
  udebug.log_detail('QueryNot<ctor>', this, 'parameter', left);
};

QueryNot.prototype = new AbstractQueryUnaryPredicate();

/******************************************************************************
 *                 CANDIDATE INDEX
 *****************************************************************************/
var CandidateIndex = function(dbTableHandler, indexNumber) {
  this.dbTableHandler = dbTableHandler;
  this.dbIndexHandler = dbTableHandler.dbIndexHandlers[indexNumber];
  udebug.log_detail('CandidateIndex<ctor> for index', this.dbIndexHandler.dbIndex.name,
      'isUnique', this.dbIndexHandler.dbIndex.isUnique, 'isOrdered', this.dbIndexHandler.dbIndex.isOrdered);
  if(! this.dbIndexHandler) {
    console.log("indexNumber", typeof(indexNumber));
    console.trace("not an index handler");
    throw new Error('Query.CandidateIndex<ctor> indexNumber is not found');
  }
  var i;
  this.numberOfColumnsInTable = this.dbTableHandler.fieldNumberToFieldMap.length;
  this.numberOfColumnsInIndex = this.dbIndexHandler.dbIndex.columnNumbers.length;
  // make an array of parameter names corresponding to index columns
  this.parameterNames = [];
  this.isOrdered = this.dbIndexHandler.dbIndex.isOrdered;
  this.isUnique = this.dbIndexHandler.dbIndex.isUnique;
  // count the number of query terms that can be used with this index
  this.columnBounds = [];
  for (i = 0; i < this.numberOfColumnsInTable; ++i) {
    this.columnBounds[i] = {};
  }
};

CandidateIndex.prototype.markEq = function(columnNumber, parameterName) {
  udebug.log_detail('CandidateIndex markEq for index', this.dbIndexHandler.dbIndex.name,
      'columnNumber', columnNumber,
      'parameterName', parameterName);
  if (this.isOrdered) {
    this.columnBounds[columnNumber].greater = true;
    this.columnBounds[columnNumber].less = true;
  } else {
    this.columnBounds[columnNumber].equal = true;
  }
  this.parameterNames[columnNumber] = parameterName;
};

CandidateIndex.prototype.markGe = function(columnNumber, parameterName) {
  udebug.log_detail('CandidateIndex markGe for index', this.dbIndexHandler.dbIndex.name,
      'columnNumber', columnNumber,
      'parameterName', parameterName);
  if (this.isOrdered) {
    // only works with ordered indexes
    this.columnBounds[columnNumber].greater = true;
    this.parameterNames[columnNumber] = parameterName;
  }
};

CandidateIndex.prototype.markLe = function(columnNumber, parameterName) {
  udebug.log_detail('CandidateIndex markLe for index', this.dbIndexHandler.dbIndex.name,
      'columnNumber', columnNumber,
      'parameterName', parameterName);
  if (this.isOrdered) {
    // only works with ordered indexes
    this.columnBounds[columnNumber].less = true;
    this.parameterNames[columnNumber] = parameterName;
  }
};

CandidateIndex.prototype.markGt = function(columnNumber, parameterName) {
  udebug.log_detail('CandidateIndex markGt for index', this.dbIndexHandler.dbIndex.name,
      'columnNumber', columnNumber,
      'parameterName', parameterName);
  if (this.isOrdered) {
    // only works with ordered indexes
    this.columnBounds[columnNumber].greater = true;
    this.parameterNames[columnNumber] = parameterName;
  }
};

CandidateIndex.prototype.markLt = function(columnNumber, parameterName) {
  udebug.log_detail('CandidateIndex markLt for index', this.dbIndexHandler.dbIndex.name,
      'columnNumber', columnNumber,
      'parameterName', parameterName);
  if (this.isOrdered) {
    // only works with ordered indexes
    this.columnBounds[columnNumber].less = true;
    this.parameterNames[columnNumber] = parameterName;
  }
};


CandidateIndex.prototype.isUsable = function(numberOfPredicateTerms) {
  var i, columnNumber;
  var numberOfMarkedColumns = 0;
  var numberOfEqualColumns = 0;
  var usable = false;
  // count the number of index columns marked
  for (i = 0; i < this.numberOfColumnsInIndex; ++i) {
    columnNumber = this.dbIndexHandler.dbIndex.columnNumbers[i];
    if (typeof(this.parameterNames[columnNumber]) !== 'undefined') {
      ++numberOfMarkedColumns;
      if (this.columnBounds[columnNumber].equal) {
        ++numberOfEqualColumns;
      }
    }
  }
  if (this.isUnique) {
    // all columns must be marked with equal to use a unique index
    if (numberOfMarkedColumns === this.numberOfColumnsInIndex
        && numberOfEqualColumns === this.numberOfColumnsInIndex
        && numberOfMarkedColumns === numberOfPredicateTerms
        ) {
      usable = true;
    }
  } else if (this.isOrdered) {
    // any columns must be marked to use a btree index
    usable = numberOfMarkedColumns > 0;
  }
  udebug.log_detail('CandidateIndex.isUsable found ', numberOfMarkedColumns,
      'marked for', this.dbIndexHandler.dbIndex.name, 'with ', this.numberOfColumnsInIndex,
      'columns in index; returning', usable);
  return usable;
};

CandidateIndex.prototype.getKeys = function(parameterValues) {
  var result = [];
  var candidateIndex = this;
  udebug.log_detail('CandidateIndex.getKeys parameters:',parameterValues,
      'candidateIndex.parameterNames', candidateIndex.parameterNames);
  // for each column in the index, get the parameter value from parameters
  this.dbIndexHandler.dbIndex.columnNumbers.forEach(function(columnNumber) {
    result.push(parameterValues[candidateIndex.parameterNames[columnNumber]]);
  });
  return result;
};

/** Evaluate candidate indexes. One point for each upper bound and lower bound. */
CandidateIndex.prototype.score = function() {
  var i;
  var score = 0;
  for (i = 0; i < this.numberOfColumnsInIndex; ++i) {
    var columnIndex = this.dbIndexHandler.dbIndex.columnNumbers[i];
    if (this.columnBounds[columnIndex].greater) {
      ++score;
    }
    if (this.columnBounds[columnIndex].less) {
      ++score;
    }
  }
  udebug.log_detail('score', this.dbIndexHandler.dbIndex.name, 'is', score);
  return score;
};

/******************************************************************************
 *                 QUERY HANDLER
 *****************************************************************************/
/* QueryHandler constructor
 * IMMEDIATE
 * 
 * statically analyze the predicate to decide whether:
 * all primary key fields are specified ==> use primary key lookup;
 * all unique key fields are specified ==> use unique key lookup;
 * some (leading) index fields are specified ==> use index scan;
 * none of the above ==> use table scan
 * Get the query handler for a given query predicate.
 * 
 */
var QueryHandler = function(dbTableHandler, predicate) {
  udebug.log_detail('QueryHandler<ctor>', util.inspect(predicate));
  this.dbTableHandler = dbTableHandler;
  this.predicate = predicate;
  var indexes = dbTableHandler.dbTable.indexes;
  // create a CandidateIndex object for each index
  // use the predicate to mark the candidate indexes
  // if the primary index is usable, choose it
  var primaryCandidateIndex = new CandidateIndex(dbTableHandler, 0);
  predicate.markCandidateIndex(primaryCandidateIndex);
  var numberOfPredicateTerms = predicate.getTopLevelPredicates().length;
  if (primaryCandidateIndex.isUsable(numberOfPredicateTerms)) {
    // we're done!
    this.candidateIndex = primaryCandidateIndex;
    this.queryType = 0; // primary key lookup
    return;
  }
  // otherwise, look for a usable unique index
  var uniqueCandidateIndex, orderedCandidateIndexes = [];
  var i, index;
  for (i = 1; i < indexes.length; ++i) {
    index = indexes[i];
    udebug.log_detail('QueryHandler<ctor> for index', index);
    if (index.isUnique) {
      // create a candidate index for unique index
      uniqueCandidateIndex = new CandidateIndex(dbTableHandler, i);
      predicate.markCandidateIndex(uniqueCandidateIndex);
      if (uniqueCandidateIndex.isUsable(numberOfPredicateTerms)) {
        this.candidateIndex = uniqueCandidateIndex;
        this.queryType = 1; // unique key lookup
        // we're done!
        return;
      }
    } else if (index.isOrdered) {
      // create an array of candidate indexes for ordered indexes to be evaluated later
      orderedCandidateIndexes.push(new CandidateIndex(dbTableHandler, i));
    } else {
      throw new Error('FatalInternalException: index is not unique or ordered... so what is it?');
    }
  }
  // otherwise, look for the best ordered index (largest number of usable query terms)
  // choose the index with the biggest score
  var topScore = 0, candidateScore = 0;
  var bestCandidateIndex = null;
  orderedCandidateIndexes.forEach(function(candidateIndex) {
    predicate.markCandidateIndex(candidateIndex);
    candidateScore = candidateIndex.score();
    if (candidateScore > topScore) {
      topScore = candidateScore;
      bestCandidateIndex = candidateIndex;
    }
  });
  if (topScore > 0) {
    this.candidateIndex = bestCandidateIndex;
    this.dbIndexHandler = bestCandidateIndex.dbIndexHandler;
    this.queryType = 2; // index scan
  } else {
    this.queryType = 3; // table scan
  }
};

/** Get key values from candidate indexes and parameters */
QueryHandler.prototype.getKeys = function(parameterValues) {
  return this.candidateIndex.getKeys(parameterValues);
};

exports.QueryDomainType = QueryDomainType;
exports.QueryHandler = QueryHandler;
