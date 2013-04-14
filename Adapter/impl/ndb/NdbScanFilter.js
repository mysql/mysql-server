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

var adapter            = require(path.join(build_dir, "ndb_adapter.node")),
    NdbInterpretedCode = adapter.ndb.ndbapi.NdbInterpretedCode,
    NdbScanFilter      = adapter.ndb.ndbapi.NdbScanFilter,
    udebug             = unified_debug.getLogger("NdbScanFilter.js");

function blah() {
  console.log("BLAH");
  console.log.apply(null, arguments);
  process.exit();
}

/* ParamLayout describes how a parameter value is encoded into a buffer
*/
function ParamLayout(column, columnId, offset, paramName) {
  this.column = column;
  this.colId  = columnId;
  this.length = column.columnSpace;
  this.offset = offset;
  this.param  = paramName;
}

/* constLayout describes the encoding of query parameter constants
*/
function constLayout(column, columnId, offset, value) {
  this.column = column;
  this.colId  = columnId;
  this.length = column.columnSpace;
  this.offset = offset;
  this.value  = value;
}

/* FilterSpec describes filter implementation; will be stored in QueryHandler
*/
function FilterSpec(predicate) {
  this.predicate       = predicate;  /* Query Predicate for this filter */
  this.isConst         = true;  /* true if filter uses constants only */
  this.constFilter = {          /* If no params are needed for filter, */
    ndbInterpretedCode : null,  /* isConst=true, and the filter is built */
    ndbScanFilter      : null   /* just once and stored here. */
  }
  this.constBuffer     = null;  /* Buffer for encoded literal constants; */
  this.constBufferSize = 0;     /* encoded just once in advance, */
  this.constLayout     = null;  /* according to this layout. */
  this.paramBufferSize = 0;     /* Buffer for encoded execution parameters. */
  this.paramLayout     = null;  /* Array of ParamLayout in buffer order */
  this.dbTable         = null;  /* NdbDictionary::Table for this filter */   
}


function getOpcode(s) {
  switch(s) {
    case ' AND ':
      return NdbScanFilter.AND;
    case ' OR ':
      return NdbScanFilter.OR;
    case ' <= ':
      return NdbScanFilter.COND_LE;
    case ' < ':
      return NdbScanFilter.COND_LT;
    case ' >= ':
      return NdbScanFilter.COND_GE;
    case ' > ':
      return NdbScanFilter.COND_GT;
    case ' = ':
      return NdbScanFilter.COND_EQ;
    case ' != ':
      return NdbScanFilter.COND_NE;      
  }
  blah(s);
}    

var groupInversions = {};
groupInversions[NdbScanFilter.AND] = NdbScanFilter.NAND;
groupInversions[NdbScanFilter.OR] = NdbScanFilter.NOR;
groupInversions[NdbScanFilter.NAND] = NdbScanFilter.AND;
groupInversions[NdbScanFilter.NOR] = NdbScanFilter.OR;

var cmpInversions = {};
cmpInversions[NdbScanFilter.COND_LE] = NdbScanFilter.COND_GT;
cmpInversions[NdbScanFilter.COND_LT] = NdbScanFilter.COND_GE;
cmpInversions[NdbScanFilter.COND_GE] = NdbScanFilter.COND_LT;
cmpInversions[NdbScanFilter.COND_GT] = NdbScanFilter.COND_LE;
cmpInversions[NdbScanFilter.COND_EQ] = NdbScanFilter.COND_NE;
cmpInversions[NdbScanFilter.COND_NE] = NdbScanFilter.COND_EQ;
cmpInversions[NdbScanFilter.COND_LIKE] = NdbScanFilter.COND_NOT_LIKE;
cmpInversions[NdbScanFilter.COND_NOT_LIKE] = NdbScanFilter.COND_LIKE;


/******************************************** OperandVisitor ************
 *
 * Visit nodes, filling in the proper operator for NdbScanFilter.
 * NdbScanFilter has no "NOT" operator, so we are also responsible for 
 * negation here.
 */
function OperandVisitor() {
  this.negating = false;
  this.size = 0;
}

/** Handle nodes QueryAnd, QueryOr */
OperandVisitor.prototype.visitQueryNaryPredicate = function(node) {
  udebug.log(node.operator);
  this.size++;
  var opcode = getOpcode(node.operator);
  if(this.negating) {
    opcode = groupInversions[opcode];
    this.negating = false;
  }
  node.ndb = { "operator" : opcode };   // scribble in the node

  for(var i = 0 ; i < node.predicates.length ; i++) {
    node.predicates[i].visit(this);
  }
}

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
OperandVisitor.prototype.visitQueryComparator = function(node) {
  udebug.log("  ", node.queryField.field.columnName, 
                   node.comparator, node.parameter.name);
  this.size++;
  var opcode = getOpcode(node.comparator);
  if(this.negating) {
    opcode = cmpInversions[opcode];
    this.negating = false;
  }
  node.ndb = { "comparator" : opcode };
}

/** Handle node QueryNot */
OperandVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  udebug.log("NOT");
  this.negating = true;
  node.predicates[0].visit(this);
}

/** Handle node QueryBetween */
OperandVisitor.prototype.visitQueryBetweenOperator = function(node) {
  blah("visitQueryUnaryOperator", node);
}

/** Handle nodes QueryIsNull, QueryIsNotNull */
OperandVisitor.prototype.visitQueryUnaryOperator = function(node) {
  blah("visitQueryBetweenOperator", node);
};


/*************************************** BufferManagerVisitor ************
 *
 * Calculate buffer layout and needed space
 */ 
function BufferManagerVisitor(dbTable) {
  this.dbTable         = dbTable;
  this.paramBufferSize = 0;
  this.paramLayout     = [];
  this.constBufferSize = 0;
  this.constLayout     = [];
}

/** Handle nodes QueryAnd, QueryOr */
BufferManagerVisitor.prototype.visitQueryNaryPredicate = function(node) {
  for(var i = 0 ; i < node.predicates.length ; i++) {
    node.predicates[i].visit(this);
  }
}

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
BufferManagerVisitor.prototype.visitQueryComparator = function(node) {
  var colId = node.queryField.field.columnNumber;
  var col = this.dbTable.columns[colId];
  var spec;
  // if param
    spec = new ParamLayout(col, colId, this.paramBufferSize, node.parameter.name);
    this.paramBufferSize += col.columnSpace;
    // this.paramBufferSize += (this.paramBufferSize % 4);   // pad for alignment
    this.paramLayout.push(spec);
  // else if constant
  node.ndb.layout = spec;   // store the layout in the query node
}

/** Handle node QueryNot */
BufferManagerVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  node.predicates[0].visit(this);
}

/** Handle node QueryBetween */
BufferManagerVisitor.prototype.visitQueryBetweenOperator = function(node) {
  blah("visitQueryUnaryOperator", node);
}

/** Handle nodes QueryIsNull, QueryIsNotNull */
BufferManagerVisitor.prototype.visitQueryUnaryOperator = function(node) {
  blah("visitQueryBetweenOperator", node);
};



/************************************** FilterBuildingVisitor ************/
function FilterBuildingVisitor(filterSpec, paramBuffer) {
  this.filterSpec         = filterSpec;
  this.paramBuffer        = paramBuffer;
  this.ndbInterpretedCode = NdbInterpretedCode.new(filterSpec.dbTable);
  this.ndbScanFilter      = NdbScanFilter.new(this.ndbInterpretedCode);
}

/** Handle nodes QueryAnd, QueryOr */
FilterBuildingVisitor.prototype.visitQueryNaryPredicate = function(node) {
  var i = 0;
  this.ndbScanFilter.begin(node.ndb.operator);
  for(i = 0 ; i < node.predicates.length ; i++) {
    node.predicates[i].visit(this);
  }
  this.ndbScanFilter.end();
};

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
FilterBuildingVisitor.prototype.visitQueryComparator = function(node) {
  var opcode = node.ndb.comparator;
  var layout = node.ndb.layout;
  this.ndbScanFilter.cmp(opcode, layout.colId, 
                         this.paramBuffer, layout.offset, layout.length);
  // TODO: constants
}

/** Handle nodes QueryNot */
FilterBuildingVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  /* The child node has already been negated by the Operand visitor */
  node.predicates[0].visit(this);
}

/** Handle nodes QueryIsNull, QueryIsNotNull */
FilterBuildingVisitor.prototype.visitQueryUnaryOperator = function(node) {
  blah("visitQueryUnaryOperator", node);
}

/** Handle node QueryBetween */
FilterBuildingVisitor.prototype.visitQueryBetweenOperator = function(node) {
  blah("visitQueryBetweenOperator", node);
}

/*************************************************/

function prepareFilterSpec(queryHandler) {
  if(queryHandler.ndbFilterSpec) return;

  var i, v;
  var spec = new FilterSpec(queryHandler.predicate);
  
  /* 1st pass: operands & negation */
  var opVisitor = new OperandVisitor();
  spec.predicate.visit(opVisitor);

  if(opVisitor.size > 0) {
    /* 2nd pass: buffer manager */
    spec.dbTable = queryHandler.dbTableHandler.dbTable;
    var bufferManager = new BufferManagerVisitor(spec.dbTable);
    spec.predicate.visit(bufferManager);

    /* Encode buffer for constant parameters */
    if(bufferManager.constBufferSize > 0) {
      spec.constBufferSize = bufferManager.constBufferSize;
      spec.constLayout     = bufferManager.constLayout;
      spec.constBuffer     = new Buffer(spec.constBufferSize);
      for(i = 0 ; i < spec.constLayout.length ; i++) {
        v = spec.constLayout[i];
        adapter.ndb.impl.encoderWrite(v.column, v.value, spec.constBuffer, v.offset);
      }
    }

    /* Assembly */
    if(bufferManager.paramBufferSize === 0) {
      var filterBuildingVisitor = new FilterBuildingVisitor(this, null);
      this.predicate.visit(filterBuildingVisitor);
      this.constFilter.ndbScanFilter = filterBuildingVisitor.ndbScanFilter;
      this.constFilter.ndbInterpretedCode = filterBuildingVisitor.ndbInterpretedCode;
    }
    else {
      spec.isConst         = false;
      spec.paramBufferSize = bufferManager.paramBufferSize;
      spec.paramLayout     = bufferManager.paramLayout;
    }
  }

  /* Attach the FilterSpec to the QueryHandler */
  queryHandler.ndbFilterSpec = spec;
}

function encodeParameters(filterSpec, params) {
  var i, f;
  var buffer = null;
  if(filterSpec.paramBufferSize) {
    buffer = new Buffer(filterSpec.paramBufferSize);
    for(i = 0; i < filterSpec.paramLayout.length ; i++) {
      f = filterSpec.paramLayout[i];
      adapter.ndb.impl.encoderWrite(f.column, params[f.param], buffer, f.offset);
    }
  }
  return buffer;
}

FilterSpec.prototype.getScanFilterCode = function(params) {
  if(this.isConst) {
    return this.constFilter.ndbInterpretedCode;
  }

  /* Encode the parameters */
  var paramBuffer = encodeParameters(this, params);

  /* Build the NdbScanFilter for this operation */
  var filterBuildingVisitor = new FilterBuildingVisitor(this, paramBuffer);
  this.predicate.visit(filterBuildingVisitor);
  
  return filterBuildingVisitor.ndbInterpretedCode;
} 


exports.prepareFilterSpec = prepareFilterSpec;
