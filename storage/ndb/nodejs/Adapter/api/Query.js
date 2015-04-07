/*
 Copyright (c) 2012, 2014, Oracle and/or its affiliates. All rights
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

var        util    = require("util");
var     BitMask    = require(mynode.common.BitMask);
var      udebug    = unified_debug.getLogger("Query.js");
var userContext    = require("./UserContext.js");

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
  return context.executeQuery(this);
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
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('QueryField<ctor>', field.fieldName);
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

// 'in' is a keyword so use alternate syntax
QueryField.prototype['in'] = function(queryParameter) {
  return new QueryIn(this, queryParameter);
};

QueryField.prototype.isNull = function() {
  return new QueryIsNull(this);
};

QueryField.prototype.isNotNull = function() {
  return new QueryIsNotNull(this);
};

QueryField.prototype.inspect = function() {
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
    if (keywords.indexOf(fieldName) === -1) {
      // field name is not a keyword
      queryDomainType[fieldName] = queryField;
    } else {
      if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('QueryDomainType<ctor> field', fieldName, 'is a keyword.');
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

QueryDomainType.prototype.inspect = function() { 
  var mynode = this.mynode_query_domain_type;
  return "[[API Query on table: " + mynode.dbTableHandler.dbTable.name + 
    ", type: " + mynode.queryType + ", predicate: " + 
    util.inspect(mynode.predicate) + "]]\n";
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
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('QueryParameter<ctor>', name);
  this.queryDomainType = queryDomainType;
  this.name = name;
};

QueryParameter.prototype.inspect = function() {
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
 *                 MARKS COLUMN MASKS IN QUERY NODES
 *****************************************************************************/
function MaskMarkerVisitor() {
}

/** Set column number in usedColumnMask */
function markUsed(node) {
  node.usedColumnMask = new BitMask(); 
  node.equalColumnMask = new BitMask();
  node.usedColumnMask.set(node.queryField.field.columnNumber);
}

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
MaskMarkerVisitor.prototype.visitQueryComparator = function(node) {
  markUsed(node);
  if(node.operationCode === 4) {  // QueryEq
    node.equalColumnMask.set(node.queryField.field.columnNumber);
  }
};

/** Nodes Between, IsNotNull, IsNotNull are all handled by markUsed() */
MaskMarkerVisitor.prototype.visitQueryUnaryOperator = markUsed;
MaskMarkerVisitor.prototype.visitQueryBetweenOperator = markUsed;

/** Handle QueryNot */
MaskMarkerVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  node.predicates[0].visit(this);
  node.equalColumnMask = new BitMask();  // Set to zero 
  node.usedColumnMask  = node.predicates[0].usedColumnMask;
};

/** Handle nodes QueryAnd, QueryOr */
MaskMarkerVisitor.prototype.visitQueryNaryPredicate = function(node) {
  var i;
  node.usedColumnMask = new BitMask(); 
  node.equalColumnMask = new BitMask();
  for(i = 0 ; i < node.predicates.length ; i++) {
    node.predicates[i].visit(this);
    node.usedColumnMask.orWith(node.predicates[i].usedColumnMask);
    if(this.operationCode === 1) {  // QueryAnd
      node.equalColumnMask.orWith(node.predicates[i].equalColumnMask);
    }
  }
};

var theMaskMarkerVisitor = new MaskMarkerVisitor();   // Singleton


/******************************************************************************
 *                 TOP LEVEL ABSTRACT QUERY PREDICATE
 *****************************************************************************/
var AbstractQueryPredicate = function() {
  this.sql = {};
};

AbstractQueryPredicate.prototype.inspect = function() {
  var str = this.operator + "(";
  this.predicates.forEach(function(value,index) { 
    if(index) str += " , ";
    str += value.inspect(); 
  });
  str += ")";
  return str;
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

AbstractQueryComparator.prototype.inspect = function() {
  return this.queryField.field.fieldName + this.comparator + this.parameter.inspect();
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

/******************************************************************************
 *                 QUERY BETWEEN
 *****************************************************************************/
QueryBetween = function(queryField, parameter1, parameter2) {
  this.comparator = ' BETWEEN ';
  this.queryField = queryField;
  this.formalParameters = [];
  this.formalParameters[0] = parameter1;
  this.formalParameters[1] = parameter2;
  this.parameter1 = parameter1;
  this.parameter2 = parameter2;
};

QueryBetween.prototype = new AbstractQueryComparator();

QueryBetween.prototype.inspect = function() {
  return this.queryField.inspect() + ' BETWEEN ' + this.parameter1.inspect() + 
    ' AND ' + this.parameter2.inspect();
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

AbstractQueryUnaryOperator.prototype.inspect = function() {
  return util.format(this);
//  return this.queryField.inspect() + this.comparator + this.parameter.inspect();
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
  this.operationCode = 7;
  this.queryField = queryField;
};

QueryIsNull.prototype = new AbstractQueryUnaryOperator();

/******************************************************************************
 *                 QUERY IS NOT NULL
 *****************************************************************************/
QueryIsNotNull = function(queryField) {
  this.operator = ' IS NOT NULL';
  this.operationCode = 8;
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
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('QueryAnd<ctor>', this);
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
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('QueryOr<ctor>', this);
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
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('QueryNot<ctor>', this, 'parameter', left);
};

QueryNot.prototype = new AbstractQueryUnaryPredicate();


/******************************************************************************
 *                 CANDIDATE INDEX
 *****************************************************************************/
// CandidateIndex is almost stateless now.
// For future consideration: move mask into DbIndexHandler, then eliminate
// CandidateIndex completely.

var CandidateIndex = function(dbTableHandler, indexNumber) {
  this.dbIndexHandler = dbTableHandler.dbIndexHandlers[indexNumber];
  if(! this.dbIndexHandler) {
    console.log("indexNumber", typeof(indexNumber));
    console.trace("not an index handler");
    throw new Error('Query.CandidateIndex<ctor> indexNumber is not found');
  }
  this.isOrdered = this.dbIndexHandler.dbIndex.isOrdered;
  this.isUnique = this.dbIndexHandler.dbIndex.isUnique;
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('CandidateIndex<ctor> for index', this.dbIndexHandler.dbIndex.name,
      'isUnique', this.isUnique, 'isOrdered', this.isOrdered);
  this.indexColumns = this.dbIndexHandler.dbIndex.columnNumbers;
  var mask = new BitMask(dbTableHandler.dbTable.columns.length);
  this.indexColumns.forEach(function(columnNumber) {
    mask.set(columnNumber);
  });
  this.mask = mask;
};

CandidateIndex.prototype.isUsable = function(predicate) {
  var usable;
  if (this.isUnique) {
    usable = predicate.equalColumnMask.and(this.mask).isEqualTo(this.mask);
  } else if(this.isOrdered) {
    usable = predicate.usedColumnMask.bitIsSet(this.indexColumns[0]);
  }
  return usable;
};


// This is used in Primary Key & Unique Key queries.
// param predicate: query predicate
// param parameterValues: the parameters object passed to query.execute()
// It returns an array, in key-column order, of the key values from the 
// parameter object.
CandidateIndex.prototype.getKeys = function(predicate, parameterValues) {

  function getParameterNameForColumn(node, columnNumber) {
    var i, name;
    if(node.equalColumnMask.bitIsSet(columnNumber)) {
      if(node.queryField && node.queryField.field.columnNumber == columnNumber) {
        return node.parameter.name;
      }
      if(node.predicates) {
        for(i = 0 ; i < node.predicates.length ; i++) {
          name = getParameterNameForColumn(node, columnNumber);
          if(name !== null) return name;
        }
      }
    }
    return null;
  }

  var result = [];
  var candidateIndex = this;
  this.indexColumns.forEach(function(columnNumber) {
    result.push(parameterValues[getParameterNameForColumn(predicate, columnNumber)]);
  });
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('CandidateIndex.getKeys parameters:', parameterValues,
                    'key:', result);
  return result;
};

/** Evaluate candidate indexes.
    Score 1 point for each consecutive key part used plus 1 more point
    if the column is in QueryEq. */
CandidateIndex.prototype.score = function(predicate) {
  var score = 0;
  var point, i;
  i = 0;
  do {
    point = predicate.usedColumnMask.bitIsSet(this.indexColumns[i]);
    if(point) { 
      score += 1; 
      if(predicate.usedColumnMask.bitIsSet(this.indexColumns[i])) {
        score += 1;
      }
    }
    i++;
  } while(point && i < this.indexColumns.length);
    
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('score', this.dbIndexHandler.dbIndex.name, 'is', score);
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
  if(udebug.is_detail()) if(udebug.is_debug()) udebug.log('QueryHandler<ctor>', util.inspect(predicate));
  this.dbTableHandler = dbTableHandler;
  this.predicate = predicate;
  var indexes = dbTableHandler.dbTable.indexes;

  // Mark the usedColumnMask and equalColumnMask in each query node
  predicate.visit(theMaskMarkerVisitor);
  
  // create a CandidateIndex object for each index
  // if the primary index is usable, choose it
  var primaryCandidateIndex = new CandidateIndex(dbTableHandler, 0);

  if(primaryCandidateIndex.isUsable(predicate)) {
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
    if (index.isUnique) {
      // create a candidate index for unique index
      uniqueCandidateIndex = new CandidateIndex(dbTableHandler, i);
      if (uniqueCandidateIndex.isUsable(predicate)) {
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
    candidateScore = candidateIndex.score(predicate);
    if (candidateScore > topScore) {
      topScore = candidateScore;
      bestCandidateIndex = candidateIndex;
    }
  });
  udebug.log("Best score is", topScore);
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
  return this.candidateIndex.getKeys(this.predicate, parameterValues);
};

exports.QueryDomainType = QueryDomainType;
exports.QueryHandler = QueryHandler;
