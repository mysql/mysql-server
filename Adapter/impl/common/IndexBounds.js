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
var udebug             = unified_debug.getLogger("IndexBounds.js");


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
   -Infinity is equivalent to NULL, and "age IS NOT NULL" is expressed 
   as "age > NULL".

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
   
   EncodedValue.compare(otherValue) should return -1, 0, or +1 according to 
   whether the stored value compares less, equal, or greater than otherValue.
*/

//////// EncodedValue             /////////////////

function EncodedValue() {
}

EncodedValue.prototype = {
  isEncodedValue : true,
  compare : function(that) {
    console.log("EncodedValue must implement compare()");
    process.exit();
  }
};


/* Utility functions 
*/
function blah() {
  console.log("BLAH");
  console.log.apply(null, arguments);
  process.exit();
}

function isBoolean(x) {
  return (typeof x === 'boolean');
}


//////// Endpoint                  /////////////////

/* An Endpoint holds a value (either EncodedValue or plain JavaScript value), 
   and, as the endpoint of a range of values, is either inclusive of the point 
   value itself or not.  "inclusive" defaults to true.
*/
function Endpoint(value, inclusive) {
  this.value = value;
  this.inclusive = (inclusive === false) ? false : true;
  this.isFinite = (typeof value === 'number') ? isFinite(value) : true;
}

Endpoint.prototype.isEndpoint = true;

Endpoint.prototype.asString = function(close) {
  var s = "";
  var value = this.value.isEncodedValue ? "<ENCODED>" : this.value;
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
   Caller is encouraged to test the return value with isBoolean().
*/
Endpoint.prototype.compare = function(that) {
  assert(that.isEndpoint);
  var cmp;

  /* First compare to infinity */
  if(this.value == -Infinity || that.value == Infinity) {
    return -1;
  }

  if(this.value == Infinity || that.value == -Infinity) {
    return 1;
  }

  if(typeof this.value === 'object' && this.value.isEncodedValue) {
    cmp = this.value.compare(that);
  }
  else {
    /* Compare JavaScript values */
    if(this.value == that.value) cmp = 0;
    else if (this.value < that.value) cmp = -1;
    else cmp = 1;
  }

  return (cmp === 0) ? (this.inclusive && that.inclusive) : cmp;
};

/* complement flips Endpoint between inclusive and exclusive
   Used in complementing number lines.
   e.g. the complement of [4,10] is [-Inf, 4) and (10, Inf]
*/
Endpoint.prototype.complement = function() {
  if(this.isFinite) 
    this.inclusive = ! this.inclusive;
};

/* Create (non-inclusive) endpoints for negative and positive infinity.
*/
var negInf = new Endpoint(-Infinity);
var posInf = new Endpoint(Infinity);

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

Segment.prototype.asString = function() {
  var s = "";
  s += this.low.asString(0) + "," + this.high.asString(1);
  return s;
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

/* compare() returns 0 if segments intersect.  Otherwise like Endpoint.compare()
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
  var s = null;
  var lp, hp, lcmp, hcmp;
  assert(that.isSegment);
  if(this.intersects(that)) {
    lcmp = this.low.compare(that.low);
    hcmp = this.high.compare(that.high);
    if(lcmp === false) {   /* Values are equal but one is exclusive. */
      lp = new Endpoint(this.low.value, false); 
    }
    else if(lcmp === 1) {
      lp = this.low;
    }
    else {
      lp = that.low;
    }
    if(hcmp === false) {  /* Values are equal but one is exclusive. */
      hp = new Endpoint(this.low.value, false);      
    }
    else if(hcmp === 1) {
      hp = this.high;
    }
    else { 
      hp = that.high;
    }
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
    lp = (this.low.compare(that.low) == -1) ? this.low : that.low;
    hp = (this.high.compare(that.high) == 1) ? this.high : that.high;
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
  var pt, segment;
  /* OperationCodes, from api/Query.js:
     LE: 0, LT: 1, GE: 2, GT: 3, EQ: 4, NE: 5 
  */
  switch(operator) {
    case 0:   // LE
      return new Segment(negInf, new Endpoint(value, true));
    case 1:   // LT 
      return new Segment(negInf, new Endpoint(value, false));
    case 2:   // GE
      return new Segment(new Endpoint(value, true), posInf);
    case 3:   // GT
      return new Segment(new Endpoint(value, false), posInf);
    case 4:   // EQ
    case 5:   // NE
      pt = new Endpoint(value);
      segment = new Segment(pt, pt);
      return (operator == 5 ? segment.complement() : segment);
    default:
      return null;
  }
}

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

NumberLine.prototype.asString = function() {
  var i, close;  
  var list = this.transitions;
  var str = "";
  for(i = 0 ; i < list.length ; i ++) {
    if(i) str += ",";
    str += list[i].asString(i % 2);
  }
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
  if(this.lowerBound().value == -Infinity) {
    this.transitions.shift();
  }
  else {
    this.transitions.unshift(negInf);
  }
  
  if(this.upperBound().value == Infinity) {
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


/****************************************** ColumnBoundVisitor ************
 *
 * Given a set of actual parameter values, 
 * calculate the bounding interval for a column
 */
function ColumnBoundVisitor(indexName, column, idxPartNo, params) {
  this.ixName = indexName;
  this.column = column;
  this.id     = idxPartNo;
  this.params = params;
  this.boundingSegment = new Segment(negInf, posInf);
}

/** Store the index bounds inside the query node */
ColumnBoundVisitor.prototype.storeRange = function(node, bounds) {
  if(node.indexRanges === undefined) {
    node.indexRanges = {};
  }
  if(node.indexRanges[this.ixName] === undefined) {
    node.indexRanges[this.ixName] = [];
  }
  node.indexRanges[this.ixName][this.id] = bounds;
};

/** Fetch index bounds stored in a query node */
ColumnBoundVisitor.prototype.fetchRange = function(node) {
  return node.indexRanges[this.ixName][this.id];
};


/** Handle nodes QueryAnd, QueryOr 
    OperationCode = 1 for AND, 2 for OR
*/
ColumnBoundVisitor.prototype.visitQueryNaryPredicate = function(node) {
  var i, line, segment;
  for(i = 0 ; i < node.predicates.length ; i++) {
    node.predicates[i].visit(this);
  }
  line = new NumberLine();

  if(node.operationCode == 1) { // AND
    line.insertSegment(this.boundingSegment);
    for(i = 0 ; i < node.predicates.length ; i++) {
      segment = this.fetchRange(node.predicates[i]);
      line.intersection(segment);
    }
  }
  else {                        // OR
    if(node.operationCode != 2) {
      blah(node);
    }
    for(i = 0 ; i < node.predicates.length ; i++) {
      segment = this.fetchRange(node.predicates[i]);
      line.union(segment);
      console.log("OR",i,segment.asString(),"=>",line.asString());
    }
  }  
  this.storeRange(node, line);
};

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
ColumnBoundVisitor.prototype.visitQueryComparator = function(node) {
  // node.queryField is the column being compared
  // node.operationCode is the comparator
  // node.parameter.name is the index into this.params 
  var segment; 

  if(node.queryField.field.columnNumber == this.column.columnNumber) {
    /* This node is a comparison on my column */
    segment = createSegmentForComparator(node.operationCode, 
                                         this.params[node.parameter.name]);
  }
  else {
    /* This node is a comparison on some other column */
    segment = this.boundingSegment;
  }
  
  /* Store it in the node */
  this.storeRange(node, segment);
};

/** Handle node QueryNot */
ColumnBoundVisitor.prototype.visitQueryUnaryPredicate = function(node) {
  var range = {};
  node.predicates[0].visit(this);
  range = this.fetchRange(node.predicates[0]);
  this.storeRange(node, range.complement());
};

/** Handle node QueryBetween */
ColumnBoundVisitor.prototype.visitQueryBetweenOperator = function(node) {
  var ep1, ep2, segment;
  
  if(node.queryField.field.columnNumber == this.column.columnNumber) {
    ep1 = this.params[node.parameter1.name];
    ep2 = this.params[node.parameter2.name];
    segment = createSegmentBetween(ep1, ep2);
  }
  else {
    segment = this.boundingSegment;
  }
  this.storeRange(node, segment);    
};

/** Handle nodes QueryIsNull, QueryIsNotNull */
ColumnBoundVisitor.prototype.visitQueryUnaryOperator = function(node) {
  var pt, segment;
  if(node.operationCode == 7) {  // NULL
    pt = new Endpoint(negInf);
    segment = new Segment(pt, pt);
  }
  else {   // IS NOT NULL, "i.e. > -Infinity"
    assert(node.operationCode == 8);
    pt = new Endpoint(negInf, false);
    segment = new Segment(pt, posInf);
  }
  this.storeRange(node, segment);
};


/* Consolidating column bounds into a full index bound

   The code above is able to evaluate a query with regard to one column;
   conditions are evaluated as Segments, and whole predicates are evaluated
   as NumberLines.
   
   After the query has been evaluated with regard to each individual column
   of the index, we must consolidate those per-column bounds into bounds
   on the whole index.
*/


//////// CompositeEndpoint         /////////////////
function CompositeEndpoint() {
  this.key = [];
  this.inclusive = true;
}

CompositeEndpoint.prototype.copy = function(that) {
  that.inclusive = this.inclusive;
  that.key = this.key.slice(0);  // slice() makes a copy
};

CompositeEndpoint.prototype.push = function(endpoint) {
  this.key.push(endpoint.value);
  this.inclusive = endpoint.inclusive;
};

CompositeEndpoint.prototype.asString = function() {
  var open, close, result, i;
  open = this.inclusive ? "[" : "(";
  close = this.inclusive ? "]" : ")";
  result = open;
  for(i = 0 ; i < this.key.length ; i++) {
    if(i) result += ",";
    result += this.key[i];
  }
  result += close;
  return result;
};

//////// IndexBounds               /////////////////

function IndexBounds() {
  this.low =  new CompositeEndpoint();
  this.high = new CompositeEndpoint();
}

IndexBounds.prototype.copy = function() {
  var that = new IndexBounds();
  this.low.copy(that.low);
  this.high.copy(that.high);
  return that;
};

IndexBounds.prototype.asString = function() {
  return "{ " + this.low.asString() + " -- " + this.high.asString() + " }";
};


//////// IndexColumn               /////////////////

function IndexColumn(resultContainer, columnBounds, nextColumn) {
  this.resultContainer = resultContainer;  // an array of finished IndexBounds
  this.columnBounds = columnBounds;        // columnBounds is a NumberLine
  this.nextColumn = nextColumn;            // nextColumn is an IndexColumn
}

/* consolidate() is a recursive algorithm, with each column 
   building its part of the bound and then passing the result along
   to the next column.
   Take the partially completed bounds object that is passed in.
   For each segment in this column's NumberLine, make a copy of
   the partialBounds, and try to add the segment to it.  If the 
   segment endpoint is exclusive, we have to stop there; otherwise,
   pass the partialBounds along to the next column.
*/
IndexColumn.prototype.consolidate = function(partialBounds, doLow, doHigh) {
  var boundsIterator, segment, idxBounds;

  boundsIterator = this.columnBounds.getIterator();
  segment = boundsIterator.next();
  console.log("consolidating", this.columnBounds.asString());
  while(segment) {
    console.log("in loop");
    idxBounds = partialBounds.copy();

    if(doLow) {
      idxBounds.low.push(segment.low);
      doLow = segment.low.inclusive && segment.low.isFinite;
    }
    if(doHigh) {
      idxBounds.high.push(segment.high);
      doHigh = segment.high.inclusive && segment.high.isFinite;
    }

    if(this.nextColumn && (doLow || doHigh)) {
      this.nextColumn.consolidate(idxBounds, doLow, doHigh);
    }
    else {
      console.log("pushing");
      this.resultContainer.push(idxBounds);
    }

    segment = boundsIterator.next();
  }
  console.log("out of loop");
};


function consolidateRanges(indexName, predicate) {
  var i, allBounds, columnBounds, thisColumn, nextColumn;

  allBounds = [];    // array of IndexBounds    
  nextColumn = null;

  function stringify() {
    var i, str='';
    for(i = 0 ; i < allBounds.length ; i++) str += allBounds[i].asString();
    return str;
  }

  for(i = predicate.indexRanges[indexName].length - 1; i >= 0 ; i--) {
    columnBounds = predicate.indexRanges[indexName][i];
    thisColumn = new IndexColumn(allBounds, columnBounds, nextColumn);
    nextColumn = thisColumn;
  }

  /* nextColumn is now the first column */  
  nextColumn.consolidate(new IndexBounds(), true, true);
  console.log("Bounds:", stringify());
  return allBounds;
}


/* getIndexBounds()

   For each column in the index, evaluate the predicate for that column.
   Then consolidate the column bounds into a set of bounds on the index.

   Returns an array of IndexBounds   

   @arg queryHandler 
   @arg dbIndex -- IndexMetadata of index to evaluate
   @arg params  -- params to substitute into query
   
   The chosen dbIndex is queryHandler.dbIndexHandler.dbIndex
   Its first column is dbIndex.columnNumbers[0] 
   The column record is queryHandler.dbTableHandler.dbTable.columns[n]      
*/
function getIndexBounds(queryHandler, dbIndex, params) {
  var nparts, i, columnNumber, column, visitors;
  visitors = [];
  nparts = dbIndex.columnNumbers.length;
  for(i = 0  ; i < nparts ; i++) {
    columnNumber = dbIndex.columnNumbers[i];
    column = queryHandler.dbTableHandler.dbTable.columns[columnNumber];
    visitors[i] = new ColumnBoundVisitor(dbIndex.name, column, i, params);
    queryHandler.predicate.visit(visitors[i]);
  }
  
  return consolidateRanges(dbIndex.name, queryHandler.predicate);
}

exports.getIndexBounds = getIndexBounds;

