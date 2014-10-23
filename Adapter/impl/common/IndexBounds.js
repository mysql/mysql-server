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
var assert     = require("assert");
var util       = require("util");
var BitMask    = require("./BitMask.js");
var udebug     = unified_debug.getLogger("IndexBounds.js");


/* Evaluation of Column Bounds from a Query

   An expression like "age < 30" defines a boundary on the value of age. 
   We can express that boundary as an interval (-Infinity, 30).
   The expression "state = 'SC'" does not define a boundary on age, 
   but nonetheless can be evaluated (with regard to age) as the interval 
   (-Infinity, +Infinity).  Knowing this, we can evaluate a query tree with 
   respect to "age" and generate an interval from every comparator node.
   
   If expressions are represented as intervals, then logical operations on 
   them can be translated into operations on the intervals:  conjunction as 
   intersection, disjunction as union, and negation as a complement.
   If comparator A returns interval Ia, and comparator B returns interval Ib,
   then the conjuction (A AND B) evaluates to the intersection of Ia and Ib.
   The disjunction (A OR B) evaluates to the union of Ia and Ib.  If Ia and Ib
   do not intersect, this union is the set {Ia, Ib}; if not, it is the one
   segment that spans from the least lower bound to the greater upper bound.

   The NOT operator evaluates to the complement of an interval.  If Ia is 
   a finite inclusive interval [0, 30], then its complement is the pair of
   exclusive intervals (-Infinity, 0) and (30, +Infinity).

   Of course -Infinity and +Infinity don't really represent infinite values,
   only the lowest and highest values of the index.  NULLs sort low, so 
   JavaScript null is equivalent to -Infinity.

   The end result is that a predicate tree, evaluated with regard to an 
   index column, is transformed into the set of ranges (segments) of the index
   which must be scanned to evaluate the predicate.
   
   Calculating the intersections and unions of intervals requires us to 
   compare two values for an indexed column.  Implementing this would be 
   easy if all indexed columns were numbers, but in fact we also have 
   at least strings and dates to deal with.  JavaScript has a funny answer 
   when you compare strings and dates to Infinity:
      ("a" < Infinity) == false
      (Date() < Infinity) == false 
   isFinite() also cannot be used with strings and dates.

   In some cases, JavaScript simply cannot compare two values at all:
   for instance, it cannot compare two strings according to particular
   MySQL collation rules.  So, we introduce the concept of an EncodedValue,
   for values that JavaScript cannot compare and whose comparison is delegated 
   elsewhere.
*/

/* Utility functions 
*/
function blah() {
  console.log("BLAH");
  console.log.apply(null, arguments);
  console.trace();
  process.exit();
}

//////// EncodedValue             /////////////////

/*
  EncodedValue.compare(otherValue) should return -1, 0, or +1 according to 
  whether the stored value compares less, equal, or greater than otherValue.
  We don't implement compare() here.
*/
function EncodedValue() {
}

EncodedValue.prototype = {
  isNonSimple    : true,
  isEncodedValue : true,  
  inspect : function() { return "<ENCODED>"; },
  compare : function() { blah("EncodedValue must implement compare()"); }
};


/* compareValues() takes two values which are either JS Values or EncodedValues.
   Returns -1, 0, or +1 as the first is less, equal to, or greater than the 
   second.
*/
function compareValues(a, b) {
  var cmp;
  
  /* First compare to infinity */
  if(a === -Infinity || a === null || b === Infinity) {
    return -1;
  }

  if(a === Infinity || b === -Infinity || b === null) {
    return 1;
  }

  if(typeof a === 'object' && a.isEncodedValue) {
    cmp = a.compare(b);
  }
  else {
    /* Compare JavaScript values */
    if(a == b) cmp = 0;
    else if (a < b) cmp = -1;
    else cmp = 1;
  }

  return cmp;
}


//////// IndexValue             /////////////////

/* IndexValue represents the multi-part value of an index.
   It is implemented as an array where each member is either a JS value or an
   EncodedValue.  Like other values, it can be used in endpoints and
   segments.
*/

function IndexValue() {
  this.size = 0;
  this.parts = [];
}

IndexValue.prototype = {
  isNonSimple      : true,
  isIndexValue     : true
};

IndexValue.prototype.pushColumnValue = function(v) {
  this.size++;
  this.parts.push(v);
};

IndexValue.prototype.copy = function() {
  var that = new IndexValue();
  that.size = this.size;
  that.parts = this.parts.slice();
  return that;
};

IndexValue.prototype.compare = function(that) {
  var n, len, cmp, v1, v2;

if(! that.isIndexValue) blah(this, that);
  assert(that.isIndexValue);
  len = this.size < that.size ? this.size : that.size;
  
  for(n = 0 ; n < len ; n++) {
    v1 = this.parts[n];
    v2 = that.parts[n];
    cmp = compareValues(v1, v2);
    if(cmp != 0) {
      return cmp;
    }
  }
  return 0; 
};

IndexValue.prototype.isFinite = function() {
  var v;
  if(this.size == 0) return false;
  v = this.parts[this.size - 1];
  return (typeof v === 'number') ?  isFinite(v) : true;
};

IndexValue.prototype.inspect = function() {
  var i, result = "Index:";
  for(i = 0 ; i < this.size ; i++) {
    if(i) result += ",";
    result += this.parts[i];
  }
  return result;
};


//////// Endpoint                  /////////////////

/* An Endpoint holds a value (plain JavaScript value, EncodedValue, 
   or IndexValue), and, as the endpoint of a range of values, is either 
   inclusive of the point value itself or not.  "inclusive" defaults to true.
*/
function Endpoint(value, inclusive) {
  this.value = value;
  this.inclusive = (inclusive === false) ? false : true;
  if(value.isIndexValue) {
    this.isFinite = value.isFinite();
  }
  else {
    this.isFinite = (typeof value === 'number') ? isFinite(value) : true;
  }
}

Endpoint.prototype.isEndpoint = true;

Endpoint.prototype.copy = function() {
  /* used only when value is an IndexValue */
  return new Endpoint(this.value.copy(), this.inclusive);
};

Endpoint.prototype.asString = function(close) {
  var s = "";
  var value = util.inspect(this.value);
  if(close) {
    s += value;
    s += (this.inclusive ? "]" : ")");
  }
  else {
    s += (this.inclusive ? "[" : "(");
    s += value;
  }
  return s;
};

/* Compare two points.
   Returns:
     -1 if this point's value is less than the other's
     +1 if this point's value is greater than the other's
     true if the values are equal and both points are inclusive
     false if the values are equal and either point is exclusive
*/
Endpoint.prototype.compare = function(that) {
  var cmp;
  assert(that.isEndpoint);
 
  if(this.value.isNonSimple) {
    cmp = this.value.compare(that.value);
  }
  else {
    cmp = compareValues(this.value, that.value);
  }

  return (cmp === 0) ? (this.inclusive && that.inclusive) : cmp;
};

/* complement flips Endpoint between inclusive and exclusive.
   Used in complementing number lines.
   e.g. the complement of [4,10] is [-Inf, 4) and (10, Inf]
*/
Endpoint.prototype.complement = function() {
  if(this.isFinite) {
    this.inclusive = ! this.inclusive;
  }
};

/* push() is used only for endpoints that contain IndexValues
*/
Endpoint.prototype.push = function(e) { 
  this.value.pushColumnValue(e.value);
  this.isFinite = e.isFinite;
  this.inclusive = e.inclusive;
};

/* Create (non-inclusive) endpoints for negative and positive infinity.
*/
var negInf = new Endpoint(-Infinity);
var posInf = new Endpoint(Infinity);


/* Functions to compare two endpoints and return one
*/
function pointGreater(e1, e2, preferInclusive) {
  switch(e1.compare(e2)) {
    case -1: 
      return e2;
    case +1:
      return e1;
    case true:
      return e1;
    case false:
      return (e1.inclusive === preferInclusive) ? e1 : e2;
  }
}

function pointLesser(e1, e2, preferInclusive) {
  switch(e1.compare(e2)) {
    case -1: 
      return e1;
    case +1:
      return e2;
    case true:
      return e1;
    case false:
      return (e1.inclusive === preferInclusive) ? e1 : e2;
  }
}


//////// Segment                   /////////////////

/* A Segment is created from two endpoints on the line.
*/
function Segment(point1, point2) {
  assert(point1.isEndpoint && point2.isEndpoint);

  if(point1.compare(point2) === -1) {
    this.low = point1;
    this.high = point2;
  }
  else {
    this.low = point2;
    this.high = point1;
  }
}

Segment.prototype.isSegment = true;

Segment.prototype.inspect = function() {
  var s = "";
  s += this.low.asString(0) + " -- " + this.high.asString(1);
  return s;
};

Segment.prototype.copy = function() {
  return new Segment(this.low.copy(), this.high.copy());
};

Segment.prototype.contains = function(point) {
  assert(point.isEndpoint);

  var hi = this.high.compare(point);
  var lo = this.low.compare(point);
  return(   ((lo === true) || (lo === -1))
         && ((hi === true) || (hi === 1)));
};

Segment.prototype.intersects = function(that) {
  assert(that.isSegment);
  
  return (   this.contains(that.low) || this.contains(that.high)
          || that.contains(this.low) || that.contains(this.high));
};

/* compare() returns 0 if segments intersect; otherwise like Endpoint.compare()
*/
Segment.prototype.compare = function(that) {  
  var r = 0; 
  assert(that.isSegment);
  if(! this.intersects(that)) {
    r = this.low.compare(that.low);
  }
  return r;
};

/* intersection: greatest lower bound & least upper bound 
*/
Segment.prototype.intersection = function(that) {
  var lp, hp, s;
  s = null;
  assert(that.isSegment);
  if(this.intersects(that)) {
    lp = pointGreater(this.low, that.low, false);
    hp = pointLesser(this.high, that.high, false);
    s = new Segment(lp, hp);
  }
  return s;
};

/* span of intersecting segments: least lower bound & greatest upper bound
*/
Segment.prototype.span = function(that) {
  var s = null;
  var lp, hp;
  if(this.intersects(that)) {
    lp = pointLesser(this.low, that.low, true);
    hp = pointGreater(this.high, that.high, true);
    s = new Segment(lp, hp);
  }
  return s;
};

/* "Iterate" over the one segment of a Segment, 
   (for compatibility with NumberLine.getIterator()
*/
function SegmentIterator(segment) {
  this.item = segment;
}

SegmentIterator.prototype.next = function() {
  var s = this.item;
  this.item = null;
  return s;
};

Segment.prototype.getIterator = function() {
  return new SegmentIterator(this);
};

/* Create a segment between two points (inclusively) 
*/
function createSegmentBetween(a, b) {
  var p1 = new Endpoint(a);
  var p2 = new Endpoint(b);
  return new Segment(p1, p2);
}

/* Create a segment for a comparison expression */
function createSegmentForComparator(operator, value) {
  var segment;
  switch(operator) {   // operation codes are from api/Query.js
    case 0:   // LE
      return new Segment(negInf, new Endpoint(value, true));
    case 1:   // LT 
      return new Segment(negInf, new Endpoint(value, false));
    case 2:   // GE
      return new Segment(new Endpoint(value, true), posInf);
    case 3:   // GT
      return new Segment(new Endpoint(value, false), posInf);
    case 4:   // EQ
      return new Segment(new Endpoint(value), new Endpoint(value));
    case 5:   // NE 
      return new Segment(new Endpoint(value), new Endpoint(value)).complement();
    default:
      return null;
  }
}

/* A segment from -Inf to +Inf
*/
var boundingSegment = new Segment(negInf, posInf);


//////// NumberLine                 /////////////////

/* A number line represents an ordered set of line segments 
   on a key space stretching from -Infinity to +Infinity
   
   The line is stored as an ordered list of transition points. 
   
   The segments on the line are from P0 to P1, P2 to P3, etc.
 
   The constructor "new NumberLine()" returns an empty NumberLine
*/

function NumberLine() {
  this.transitions = [];
} 

NumberLine.prototype.isNumberLine = true;

NumberLine.prototype.inspect = function() {
  var it, str, segment;
  it = this.getIterator();
  str = "{ ";
  while((segment = it.next()) !== null) {
    if(it.n > 2) str += ",";
    str += segment.inspect();
  }
  str += " }";
  return str;  
};

NumberLine.prototype.isEmpty = function() {
  return (this.transitions.length == 0);
};

NumberLine.prototype.setAll = function() {
  this.transitions = [ negInf, posInf ];
  return this;
};

NumberLine.prototype.setEqualTo = function(that) {
  this.transitions = that.transitions;
};

NumberLine.prototype.upperBound = function() {
  if(this.isEmpty()) return negInf;
  return this.transitions[this.transitions.length - 1];
};

NumberLine.prototype.lowerBound = function() {
  if(this.isEmpty()) return posInf;
  return this.transitions[0];
};

/* A NumberLineIterator can iterate over the segments of a NumberLine 
*/
function NumberLineIterator(numberLine) {
  this.line = numberLine;
  this.list = numberLine.transitions;
  this.n = 0;
}

NumberLineIterator.prototype.next = function() {
  var s = null;
  if(this.n < this.list.length) {
    s = new Segment(this.list[this.n], this.list[this.n+1]);
    this.n += 2;
  }
  return s;    
};

/* An iterator's splice point is the index into a NumberLine's transition list
   just prior to the most recently read segment. 
   Returns null if the iterator has not yet been read. 
*/
NumberLineIterator.prototype.getSplicePoint = function() {
  var idx = null;
  if(this.n >= 2) {  
    idx = this.n - 2;
  }
  return idx; 
};

NumberLine.prototype.getIterator = function() { 
  return new NumberLineIterator(this);
};


/* Complement of a number line (Negation of an expression)
*/
NumberLine.prototype.complement = function() {
  if(! this.lowerBound().isFinite) {
    this.transitions.shift();
  }
  else {
    this.transitions.unshift(negInf);
  }
  
  if(! this.upperBound().isFinite) {
    this.transitions.pop();
  }
  else {
    this.transitions.push(posInf);
  }

  assert(this.transitions.length % 2 == 0);

  this.transitions.forEach(function(p) { p.complement(); });

  return this;
};

/* Insert a segment into a number line.
   Assume as a given that the segment does not intersect any existing one.
*/
NumberLine.prototype.insertSegment = function(segment) {
  var i, s, sp;
  /* Segment is above the current upper bound */
  if(this.upperBound().compare(segment.low) < 0) {
    this.transitions.push(segment.low);
    this.transitions.push(segment.high);
  }
  else { 
    /* Sorted Insertion */
    i = this.getIterator();
    while((s = i.next()) !== null) {
      if(s.compare(segment) == 1) { 
        sp = i.getSplicePoint(); 
        this.transitions = 
          this.transitions.slice(0, sp) .
          concat(segment.low)    .
          concat(segment.high)   .
          concat(this.transitions.slice(sp));
      }
    }
  }
};

/* Mutable: sets line equal to the intersection of line with segment
*/
NumberLine.prototype.intersectWithSegment = function(segment) {
  var it, line, s;
  it = this.getIterator();
  line = new NumberLine();
  while((s = it.next()) !== null) {
    if(s.intersects(segment)) {
      line.insertSegment(s.intersection(segment));
    }
  }
  this.setEqualTo(line);
};

/* Mutable: sets line equal to the intersection of line and that 
*/
NumberLine.prototype.intersection = function(that) {
  var it, s;
  it = that.getIterator();
  while((s = it.next()) !== null) {
    this.intersectWithSegment(s);
  }
};

/* UNION, "X < 4 OR X > 6" 
   Mutable.
*/
NumberLine.prototype.unionWithSegment = function(segment) {
  var it, line, s, overlaps;
  it = this.getIterator();
  line = new NumberLine();
  overlaps = [];
  /* Sort our own segments into two groups: 
     those that overlap segment, and those that don't 
  */
  while((s = it.next()) !== null) {
    if(s.intersects(segment)) {
      overlaps.push(s);
    }
    else {
      line.insertSegment(s);
    }
  }
  s = segment;
  if(overlaps.length) s = s.span(overlaps.shift());  /* leftmost */
  if(overlaps.length) s = s.span(overlaps.pop()); /* rightmost */
  line.insertSegment(s);

  this.setEqualTo(line);
};

/* Mutable: changes this into Union (this, that)
*/
NumberLine.prototype.union = function(that) {
  var it, s;
  it = that.getIterator();
  while((s = it.next()) !== null) {
    this.unionWithSegment(s);
  }
};

/* createNumberLineFromSegment()
*/
function createNumberLineFromSegment(segment) {
  var line = new NumberLine();
  line.transitions[0] = segment.low;
  line.transitions[1] = segment.high;
  return line;
}

/* Complement a Segment by turning it into a NumberLine
*/
Segment.prototype.complement = function() {
  return createNumberLineFromSegment(this).complement();
};


/* Returns the columnBound already stored in the node,
   or the boundingSegment.
*/
function evaluateNodeForColumn(node, columnNumber) {
  var segment = node.columnBound[columnNumber];
  if(typeof segment === 'undefined') {
    segment = boundingSegment;
  }
  return segment;
}

/* Returns a NumberLine 
*/
function intersectionForColumn(predicates, columnNumber) {
  var i, line;
  line = new NumberLine().setAll();

  for(i = 0 ; i < predicates.length ; i++) {
    line.intersection(evaluateNodeForColumn(predicates[i], columnNumber));
  }
  return line;
}

/* Returns a NumberLine
*/
function unionForColumn(predicates, columnNumber) { 
  var i, line;
  line = new NumberLine();
  
  for(i = 0 ; i < predicates.length ; i++) {
    line.union(evaluateNodeForColumn(predicates[i], columnNumber));
  }
  return line;
}

/******** This is a map operationCode => function 
*/
var queryNaryFunctions = [ null , intersectionForColumn , unionForColumn ] ;


/****************************************** ColumnBoundVisitor ************
 *
 * Given a set of actual parameter values, visit the query tree and store
 * a segment for every comparator, BETWEEN, and IS NULL / NOT NULL node.
 * 
 * For grouping nodes AND, OR, NOT, store the intersection, union, or complement
 * over the nodes grouped, for every column referenced in that group.
 *
 *
 */
function ColumnBoundVisitor(params) {
  this.params = params;
}

/* Store a segment at a node
*/
ColumnBoundVisitor.prototype.store = function(node, segment) {
  node.columnBound = {};
  node.columnBound[node.queryField.field.columnNumber] = segment;
};

/** AND/OR nodes */
ColumnBoundVisitor.prototype.visitQueryNaryPredicate = function(node) {
  var i, c, unionOrIntersection, doColumns;
  unionOrIntersection = queryNaryFunctions[node.operationCode]; 

  for(i = 0 ; i < node.predicates.length ; i++) {
    node.predicates[i].visit(this);
  }
  node.columnBound = {};
  doColumns = node.usedColumnMask.toArray();
  while(doColumns.length) {
    c = doColumns.pop();
    node.columnBound[c] = unionOrIntersection(node.predicates, c);
  }
};

/** NOT node */
ColumnBoundVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  var c, doColumns;
  node.predicates[0].visit(this);
  doColumns = node.usedColumnMask.toArray();
  node.columnBound = {};
  while(doColumns.length) {
    c = doColumns.pop();
    node.columnBound[c] = node.predicates[0].columnBound[c].complement();
  }
};

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
ColumnBoundVisitor.prototype.visitQueryComparator = function(node) {
  var segment = createSegmentForComparator(node.operationCode, 
                                           this.params[node.parameter.name]);
  this.store(node, segment);
};

/** Handle node QueryBetween */
ColumnBoundVisitor.prototype.visitQueryBetweenOperator = function(node) {
  var ep1, ep2, segment;  
  ep1 = this.params[node.parameter1.name];
  ep2 = this.params[node.parameter2.name];
  segment = createSegmentBetween(ep1, ep2);
  this.store(node, segment);
};

/** Handle nodes QueryIsNull, QueryIsNotNull */
ColumnBoundVisitor.prototype.visitQueryUnaryOperator = function(node) {
  var pt, segment;
  if(node.operationCode == 7) {  // NULL
    pt = new Endpoint(null);
    segment = new Segment(pt, pt);
  }
  else {   // NOT NULL
    assert(node.operationCode == 8);
    pt = new Endpoint(null, false);
    segment = new Segment(pt, posInf);
  }
  this.store(node, segment);
};



/****************************************** IndexBoundVisitor ************
 *
 * Visit a tree that has already been marked by a ColumnBoundVisitor.
 * Construct a set of IndexBounds for a particular index.
 */
function IndexBoundVisitor(queryHandler, dbIndex) {
  this.index = dbIndex;
  this.ncol = this.index.columnNumbers.length;
  this.mask = new BitMask(queryHandler.dbTableHandler.dbTable.columns.length);
  if(this.ncol > 1) {
    this.mask.set(this.index.columnNumbers[0]);
    this.mask.set(this.index.columnNumbers[1]);
  }
}

/* A consolidated index bound for a multi-column index is a set of 
   segments built from IndexValues.   Each single-column value is 
   represented by a Consolidator that builds its part of the 
   conslidated index bound.

   Take the partially completed bounds object that is passed in.
   For each segment in this column's own bound, make a copy of
   the partialBounds, and try to add the segment to it.  If the 
   segment endpoint is exclusive or infinite, stop there; otherwise, 
   pass the partialBounds along to the next column.
*/

var initialIndexBounds = 
  new Segment(new Endpoint(new IndexValue()), new Endpoint(new IndexValue()));

IndexBoundVisitor.prototype.consolidate = function(node) {
  var i, allBounds, columnBounds, thisColumn, nextColumn;

  function Consolidator(columnBounds, nextColumnConsolidator) {
    this.columnBounds = columnBounds; // bounds of this column
    this.nextColumnConsolidator = nextColumnConsolidator; 
  }

  Consolidator.prototype.consolidate = function(partialBounds, doLow, doHigh) {
    var boundsIterator, segment, idxBounds, doNextLow, doNextHigh;

    doNextLow = doLow;
    doNextHigh = doHigh;

    boundsIterator = this.columnBounds.getIterator();
    segment = boundsIterator.next();
    while(segment) {
      idxBounds = partialBounds.copy();

      if(doLow) {
        idxBounds.low.push(segment.low);
        doNextLow = segment.low.inclusive && segment.low.isFinite;
      }
      if(doHigh) {
        idxBounds.high.push(segment.high);
        doNextHigh = segment.high.inclusive && segment.high.isFinite;
      }

      if(this.nextColumnConsolidator && (doNextLow || doNextHigh)) {
        this.nextColumnConsolidator.consolidate(idxBounds, doNextLow, doNextHigh);
      }
      else {
        allBounds.insertSegment(idxBounds);
      }

      segment = boundsIterator.next();
    }
  };

  if(! node.indexRange) {
    allBounds = new NumberLine();
    nextColumn = null;

    for(i = this.ncol - 1; i >= 0 ; i--) {
      columnBounds = evaluateNodeForColumn(node, this.index.columnNumbers[i]);
      udebug.log("consolidate predicate:", node);
      udebug.log("consolidate in:", i, columnBounds);
      thisColumn = new Consolidator(columnBounds, nextColumn);
      nextColumn = thisColumn;
    }
    /* nextColumn is now the first column */
    nextColumn.consolidate(initialIndexBounds, true, true);
    udebug.log("consolidate out:", allBounds);
    node.indexRange = allBounds;
  }
};


/** Handle nodes QueryAnd, QueryOr 
    OperationCode = 1 for AND, 2 for OR
*/
IndexBoundVisitor.prototype.visitQueryNaryPredicate = function(node) {
  var i, indexRange;
  var buildChildNodeIndexBounds = false;

  if(node.usedColumnMask.and(this.mask).isEqualTo(this.mask)) {
    for(i = 0 ; i < node.predicates.length ; i++) {
      node.predicates[i].visit(this);
      if(node.predicates[i].indexRange) { buildChildNodeIndexBounds = true; }
    }
    if(buildChildNodeIndexBounds) {
      // If any child node is consolidated, consolidate all of them, 
      // then construct the union or intersection over the consolidated bounds.
      indexRange = new NumberLine();
      for(i = 0 ; i < node.predicates.length ; i++) {
        this.consolidate(node.predicates[i]);
        switch(node.operationCode) {
          case 1:
            indexRange.intersection(node.predicates[i].indexRange);
            udebug.log("Intersection: ", indexRange);
            break;
          case 2:
            indexRange.union(node.predicates[i].indexRange);
            udebug.log("Union: ", indexRange);
            break;
        }
      }
      node.indexRange = indexRange;
    } else {
      this.consolidate(node);
    }
  }
};

/** Handle node QueryNot */
IndexBoundVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  if(node.usedColumnMask.and(this.mask).isEqualTo(this.mask)) {
    node.predicates[0].visit(this);
    if(node.predicates[0].indexRange) {
      node.indexRange = node.predicates[0].indexRange.complement();
    }
  }
};

/* Construct an exported IndexBound from a segment
*/
function IndexBound(segment) {
  function IndexBoundEndpoint(endpoint) {
    this.inclusive = endpoint.inclusive;
    this.key = endpoint.value.parts;
  }
  this.low = new IndexBoundEndpoint(segment.low);
  this.high = new IndexBoundEndpoint(segment.high);
}

/* getIndexBounds()

   For each column in the index, evaluate the predicate for that column.
   Then consolidate the column bounds into a set of bounds on the index.

   @arg queryHandler:  query 
   @arg dbIndex:       IndexMetadata of index to evaluate
   @arg params:        params to substitute into query

   Returns an array of IndexBounds   
*/
function getIndexBounds(queryHandler, dbIndex, params) {
  var indexVisitor, queryIndexRange, it, segment, bounds;

  /* Evaluate the query tree using the actual parameters */
  queryHandler.predicate.visit(new ColumnBoundVisitor(params));

  /* Then analyze it for this particular index */
  indexVisitor = new IndexBoundVisitor(queryHandler, dbIndex);
  queryHandler.predicate.visit(indexVisitor);

  /* Build a consolidated index bound for the top-level predicate */
  indexVisitor.consolidate(queryHandler.predicate);
  queryIndexRange = queryHandler.predicate.indexRange;
  udebug.log("Index range for query:", queryIndexRange);

  /* Transform NumberLine to array of IndexBound */
  bounds = [];
  it = queryIndexRange.getIterator();
  while((segment = it.next()) !== null) {
    bounds.push(new IndexBound(segment));
  }
  
  return bounds;  
}

exports.getIndexBounds = getIndexBounds;

