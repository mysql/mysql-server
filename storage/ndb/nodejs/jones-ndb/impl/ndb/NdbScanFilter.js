/*
 Copyright (c) 2013, 2024, Oracle and/or its affiliates.

 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is designed to work with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have either included with
 the program or referenced in the documentation.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

"use strict";

var conf = require("./path_config"), assert = require("assert"),
    adapter = require(conf.binary),
    NdbInterpretedCode = adapter.ndb.ndbapi.NdbInterpretedCode,
    NdbScanFilter = adapter.ndb.ndbapi.NdbScanFilter,
    udebug = unified_debug.getLogger("NdbScanFilter.js");


function QueryTerm(offset, column, param) {
  this.param = param;
  this.column = column;
  this.offset = offset;
  this.constBuffer = null;
}

/* Encode value into buffer.
   If params are supplied, then this.param is assumed to be a QueryParameter.
   Otherwise, this.param is treated as a query constant term, and we retain
   a reference to the buffer.
*/
QueryTerm.prototype.encode = function(buffer, params) {
  var value;
  if (params) {
    value = params[this.param.name];  // a QueryParameter (from Jones Query.js)
  } else {
    value = this.param;  // a query constant
    this.constBuffer = buffer;
  }
  adapter.ndb.impl.encoderWrite(this.column, value, buffer, this.offset);
};


/* BufferSchema points to a Buffer and describes the size and layout of
   values in that buffer.
*/
function BufferSchema() {
  this.layout = [];  // an array of QueryTerm
  this.size = 0;     // length of the buffer
}

/* Create a buffer;
   encode each spec in layout into the buffer using the supplied params;
   return the buffer.
*/
BufferSchema.prototype.encode = function(params) {
  var i, buffer;
  buffer = null;

  if (this.size > 0) {
    buffer = Buffer.alloc(this.size);
    for (i = 0; i < this.layout.length; i++) {
      this.layout[i].encode(buffer, params);
    }
  }
  return buffer;
};

/* Add a query term to layout, and return it
 */
BufferSchema.prototype.addTerm = function(column, param) {
  var term = new QueryTerm(this.size, column, param);
  this.layout.push(term);
  this.size += column.columnSpace;
  return term;
};


/* Make a note in a node of the predicate tree.
   The note will be used to store all NDB-related analysis.
   Copy the node's operator or comparator code into the ndb section.
   This should be called in the first-pass visitor.
*/
function markNode(node) {
  var opcode = node.operationCode || null;
  node.ndb = {"opcode": opcode, "layout": null, "intervals": {}};
}


/*************************************** BufferManagerVisitor ************
 *
 * This is the first pass, run when the operation is declared.
 *
 * Visit nodes, marking them for NdbScanFilter.  Calculate buffer layout
 * and needed space.
 */
function BufferManagerVisitor(filterSpec) {
  this.spec = filterSpec;
}

/** Handle nodes QueryAnd, QueryOr */
BufferManagerVisitor.prototype.visitQueryNaryPredicate = function(node) {
  var i;
  markNode(node);
  for (i = 0; i < node.predicates.length; i++) {
    node.predicates[i].visit(this);
  }
};

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
BufferManagerVisitor.prototype.visitQueryComparator = function(node) {
  var colId = node.queryField.field.columnNumber;
  var col = this.spec.dbTable.columns[colId];
  var schema = node.constants ? this.spec.constSchema : this.spec.paramSchema;
  markNode(node);
  node.ndb.layout = schema.addTerm(col, node.parameter);
};

/** Handle node QueryNot */
BufferManagerVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  markNode(node);
  node.predicates[0].visit(this);
};

/** Handle node QueryBetween */
BufferManagerVisitor.prototype.visitQueryBetweenOperator = function(node) {
  var colId, col, spec1, spec2, schema1, schema2;
  colId = node.queryField.field.columnNumber;
  col = this.spec.dbTable.columns[colId];
  schema1 = node.constants & 1 ? this.spec.constSchema : this.spec.paramSchema;
  schema2 = node.constants & 2 ? this.spec.constSchema : this.spec.paramSchema;
  spec1 = schema1.addTerm(col, node.parameter1);
  spec2 = schema2.addTerm(col, node.parameter2);

  markNode(node);
  node.ndb.layout = {"between": [spec1, spec2]};
};

/** Handle nodes QueryIsNull, QueryIsNotNull */
BufferManagerVisitor.prototype.visitQueryUnaryOperator = function(node) {
  markNode(node);
  node.ndb.layout = {"columnNumber": node.queryField.field.columnNumber};
};


/************************************** FilterBuildingVisitor ************
 *
 * This is the second pass, run each time the operation is executed.
 *
 * Visit nodes and build NdbScanFilter.
 */
function FilterBuildingVisitor(dbTable, paramBuffer) {
  this.paramBuffer = paramBuffer;
  this.ndbInterpretedCode = NdbInterpretedCode.create(dbTable);
  this.ndbScanFilter = NdbScanFilter.create(this.ndbInterpretedCode);
  this.ndbScanFilter.begin(1);  // implicit top-level AND group
}

/** Handle nodes QueryAnd, QueryOr */
FilterBuildingVisitor.prototype.visitQueryNaryPredicate = function(node) {
  var i = 0;
  this.ndbScanFilter.begin(node.ndb.opcode);
  for (i = 0; i < node.predicates.length; i++) {
    node.predicates[i].visit(this);
  }
  udebug.log(node.operator);
  this.ndbScanFilter.end();
};

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
FilterBuildingVisitor.prototype.visitQueryComparator = function(node) {
  var opcode = node.ndb.opcode;
  var layout = node.ndb.layout;
  this.ndbScanFilter.cmp(
      opcode, layout.column.columnNumber,
      layout.constBuffer || this.paramBuffer, layout.offset,
      layout.column.columnSpace);
  udebug.log(node.queryField.field.fieldName, node.comparator, "value");
};

/** Handle nodes QueryNot */
FilterBuildingVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  this.ndbScanFilter.begin(node.ndb.opcode);  // A 1-member NAND group
  node.predicates[0].visit(this);
  udebug.log("NOT");
  this.ndbScanFilter.end();
};

/** Handle nodes QueryIsNull, QueryIsNotNull */
FilterBuildingVisitor.prototype.visitQueryUnaryOperator = function(node) {
  var opcode = node.ndb.opcode;
  var colId = node.ndb.layout.columnNumber;

  if (opcode === 7) {
    this.ndbScanFilter.isnull(colId);
  } else {
    assert(opcode === 8);
    this.ndbScanFilter.isnotnull(colId);
  }
  udebug.log(node.queryField.field.fieldName, node.operator);
};

/** Handle node QueryBetween */
FilterBuildingVisitor.prototype.visitQueryBetweenOperator = function(node) {
  var col1 = node.ndb.layout.between[0];
  var col2 = node.ndb.layout.between[1];
  this.ndbScanFilter.begin(1);  // AND
  this.ndbScanFilter.cmp(
      2, col1.column.columnNumber, col1.constBuffer || this.paramBuffer,
      col1.offset, col1.column.columnSpace);  // >= col1
  this.ndbScanFilter.cmp(
      0, col2.column.columnNumber, col2.constBuffer || this.paramBuffer,
      col2.offset, col2.column.columnSpace);  // <= col2
  this.ndbScanFilter.end();
  udebug.log(node.queryField.field.fieldName, "BETWEEN values");
};

FilterBuildingVisitor.prototype.finalise = function() {
  this.ndbScanFilter.end();
};

/*************************************************/

/* FilterSpec describes filter implementation; will be stored in QueryHandler
 */
function FilterSpec(queryHandler) {
  this.predicate = queryHandler.predicate;
  this.dbTable = queryHandler.dbTableHandler.dbTable;
  this.constSchema = new BufferSchema();
  this.paramSchema = new BufferSchema();
  this.constFilter = null;
  this.constBuffer = null;
  this.markQuery();
}

FilterSpec.prototype.markQuery = function() {
  /* 1st pass.  Mark tree and calculate buffer sizes. */
  this.predicate.visit(new BufferManagerVisitor(this));

  /* Encode buffer for constant query terms */
  if (this.predicate.constants) {
    this.constBuffer = this.constSchema.encode();

    /* If paramSchema.size is zero, then the query uses *only* constant terms.
       Optimize by building a filter just once in advance.
    */
    if (this.paramSchema.size === 0) {
      this.constFilter = this.buildFilter(null);
    }
  }
};

FilterSpec.prototype.buildFilter = function(paramBuffer) {
  var visitor = new FilterBuildingVisitor(this.dbTable, paramBuffer);
  this.predicate.visit(visitor);
  visitor.finalise();
  return visitor;
};

FilterSpec.prototype.getScanFilterCode = function(params) {
  var paramBuffer;

  if (this.constFilter) {
    udebug.log("getScanFilterCode: ScanFilter is const");
    return this.constFilter.ndbInterpretedCode;
  }

  /* Encode the parameters */
  paramBuffer = this.paramSchema.encode(params);

  /* Build the NdbScanFilter for this operation */
  return this.buildFilter(paramBuffer).ndbInterpretedCode;
};


function prepareFilterSpec(queryHandler) {
  if (queryHandler.predicate && !queryHandler.ndbFilterSpec) {
    queryHandler.ndbFilterSpec = new FilterSpec(queryHandler);
  }
}

exports.prepareFilterSpec = prepareFilterSpec;
