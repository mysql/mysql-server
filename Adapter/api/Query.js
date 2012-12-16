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

/*global unified_debug */

"use strict";

var     udebug     = unified_debug.getLogger("Query.js");
var userContext    = require('../impl/common/UserContext.js');

/** Query Domain Type represents a domain object that can be used to create and execute queries.
 * It encapsulates the dbTableHandler (obtained from the domain object or table name),
 * the session (required to execute the query), and the filter which limits the result.
 * @param session the user Session
 * @param dbTableHandler the dbTableHandler
 * @param domainObject true if the query results are domain objects
 */
var QueryDomainType = function(session, dbTableHandler, domainObject) {
  udebug.log('QueryDomainType<ctor>', dbTableHandler.dbTable.name);
  // avoid name conflicts: put all implementation artifacts into the property mynode_query_domain_type
  this.mynode_query_domain_type = {};
  var mynode = this.mynode_query_domain_type;
  mynode.session = session;
  mynode.dbTableHandler = dbTableHandler;
  mynode.domainObject = domainObject;
  var queryDomainType = this;
  var fieldName;
  // add a property for each field in the table mapping
  mynode.dbTableHandler.fieldNumberToFieldMap.forEach(function(field) {
    fieldName = field.fieldName;
    queryDomainType[fieldName] = new QueryField(queryDomainType, field);
  });
};

/**
 * QueryParameter represents a named parameter for a query. The QueryParameter marker is used
 * as the comparand for QueryField.
 * @param queryDomainType
 * @param name
 * @return
 */
var QueryParameter = function(queryDomainType, name) {
  udebug.log_detail('QueryParameter<ctor>', name);
  this.queryDomainType = queryDomainType;
  this.name = name;
};

QueryParameter.prototype.toString = function() {
  return '?' + this.name;
};

/**
 * QueryField represents a mapped field in a domain object. QueryField is used to build
 * QueryPredicates by comparing the field to parameters.
 * @param queryDomainType
 * @param field
 * @return
 */
var QueryField = function(queryDomainType, field) {
  udebug.log_detail('QueryField<ctor>', field.fieldName);
  this.queryDomainType = queryDomainType;
  this.field = field;
};

QueryField.prototype.eq = function(queryParameter) {
  return new QueryEq(this, queryParameter);
};

QueryField.prototype.toString = function() {
  udebug.log_detail('QueryField.toString: ', this.field.fieldName);
  return this.field.fieldName;
};

/** AbstractQueryPredicate is the top level Predicate */
var AbstractQueryPredicate = function() {
};

AbstractQueryPredicate.prototype.markCandidateIndex = function(candidateIndex) {
  var topLevelPredicates = this.getTopLevelPredicates();
  topLevelPredicates.forEach(function(predicate) {
    predicate.mark(candidateIndex);
  }); 
};

AbstractQueryPredicate.prototype.mark = function(candidateIndex) {
  throw new Error('FatalInternalException: abstract method mark(candidateIndex)');
};

AbstractQueryPredicate.prototype.and = function(predicate) {
  // TODO validate parameter
  return new QueryAnd(this, predicate);
};

AbstractQueryPredicate.prototype.getTopLevelPredicates = function() {
  return [this];
};

var AbstractQueryComparator = function() {
};

/** AbstractQueryComparator inherits AbstractQueryPredicate */
AbstractQueryComparator.prototype = new AbstractQueryPredicate();

AbstractQueryComparator.prototype.toString = function() {
  return this.queryField.toString() + ' ' + this.comparator + ' ' + this.parameter.toString();
};

var QueryEq = function(queryField, parameter) {
  this.comparator = '=';
  this.queryField = queryField;
  this.parameter = parameter;
};

/** QueryEq inherits AbstractQueryComparator */
QueryEq.prototype = new AbstractQueryComparator();

QueryEq.prototype.mark = function(candidateIndex) {
  var columnNumber = this.queryField.field.columnNumber;
  var parameterName = this.parameter.name;
  udebug.log_detail('QueryEq.mark with columnNumber:', columnNumber, 'parameterName:', parameterName);
  candidateIndex.markEq(columnNumber, parameterName);
};

var QueryAnd = function(left, right) {
  udebug.log_detail('QueryAnd<ctor>');
  this.operator = 'and';
  this.predicates = [left, right];
};

/** QueryAnd inherits AbstractQueryPredicate */
QueryAnd.prototype = new AbstractQueryPredicate();

/** Override the "and" function to collect all predicates in one variable.
 * 
 * @param predicate
 * @return
 */
QueryAnd.prototype.and = function(predicate) {
  this.predicates.push(predicate);
};

QueryAnd.prototype.getTopLevelPredicates = function() {
  return predicates;
};

QueryDomainType.prototype.param = function(name) {
  return new QueryParameter(this, name);
};

QueryDomainType.prototype.where = function(predicate) {
  var mynode = this.mynode_query_domain_type;
  mynode.predicate = predicate;
  mynode.queryHandler = new QueryHandler(mynode.dbTableHandler, predicate);
  mynode.queryType = mynode.queryHandler.queryType;
  return this;
};

QueryDomainType.prototype.execute = function() {
  var session = this.mynode_query_domain_type.session;
  var context = new userContext.UserContext(arguments, 2, 2, session, session.sessionFactory);
  // delegate to context's execute for execution
  context.executeQuery(this);
};

var CandidateIndex = function(dbTableHandler, indexNumber) {
  this.dbTableHandler = dbTableHandler;
  this.dbIndexHandler = dbTableHandler.dbIndexHandlers[indexNumber];
  if(! this.dbIndexHandler) {
    console.log("indexNumber", typeof(indexNumber));
    console.trace("not an index handler");
    process.exit();
  }
  
  this.numberOfColumns = this.dbIndexHandler.dbIndex.columnNumbers.length;
  // make an array of parameter names corresponding to index columns
  this.parameterNames = [];
  udebug.log_detail('CandidateIndex<ctor> for index', this.dbIndexHandler.dbIndex.name);
};

CandidateIndex.prototype.markEq = function(columnNumber, parameterName) {
  this.parameterNames[columnNumber] = parameterName;
};

CandidateIndex.prototype.isUsable = function() {
  udebug.log_detail('CandidateIndex.isUsable for', this.dbIndexHandler.dbIndex.name, 'with', this.parameterNames);
  var i, columnNumber;
  var numberOfMarkedColumns = 0;
  // count the number of index columns marked
  for (i = 0; i < this.numberOfColumns; ++i) {
    columnNumber = this.dbIndexHandler.dbIndex.columnNumbers[i];
    if (typeof(this.parameterNames[columnNumber]) !== 'undefined') {
      ++numberOfMarkedColumns;
    }
  }
  udebug.log_detail('CandidateIndex.isUsable found ', numberOfMarkedColumns, 'marked');
  return numberOfMarkedColumns === this.numberOfColumns;
};

CandidateIndex.prototype.getKeys = function(parameters) {
  var result = [];
  var candidateIndex = this;
  udebug.log_detail('CandidateIndex.getKeys parameters:',parameters, 'candidateIndex.parameterNames', candidateIndex.parameterNames);
  // for each column in the index, get the parameter value from parameters
  this.dbIndexHandler.dbIndex.columnNumbers.forEach(function(columnNumber) {
    result.push(parameters[candidateIndex.parameterNames[columnNumber]]);
  });
  return result;
};

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
  this.dbTableHandler = dbTableHandler;
  this.predicate = predicate;
  var indexes = dbTableHandler.dbTable.indexes;
  // create a CandidateIndex object for each index
  // use the predicate to mark the candidate indexes
  // if the primary index is usable, choose it
  var primaryCandidateIndex = new CandidateIndex(dbTableHandler, 0);
  predicate.markCandidateIndex(primaryCandidateIndex);
  if (primaryCandidateIndex.isUsable()) {
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
      if (uniqueCandidateIndex.isUsable()) {
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
    this.candidateIndex = bestCandidateInstance;
    this.queryType = 2; // index scan
  } else {
    this.queryType = 3; // table scan
  }
};

/** Get key values from candidate indexes and parameters */
QueryHandler.prototype.getKeys = function(parameters) {
  return this.candidateIndex.getKeys(parameters);
};

exports.QueryDomainType = QueryDomainType;
exports.QueryHandler = QueryHandler;
