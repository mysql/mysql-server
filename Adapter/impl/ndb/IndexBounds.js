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


/* Evaluation of IndexBounds from a Query

   An expression like "age < 30" defines a boundary on the value of age. 
   We can express that boundary as an interval (-Infinity, 30).
   The expression "state = 'SC'" does not define a boundary on age, 
   but nonetheless can be evaluated (with regard to age) as the interval 
   (-Infinity, +Infinity).  Knowing this, we can evaluate a query tree with 
   respect to "age" and generate an interval from every comparator node.
   
   If comparator A returns interval Ia, and comparator B returns interval Ib,
   then the conjuction (A AND B) evaluates to the intersection of Ia and Ib.
   The disjunction (A OR B) evaluates to the union of Ia and Ib.  If Ia and Ib
   do not intersect, this union is the set {Ia, Ib}; if not, it is the one
   segment that spans from the least lower bound to the greater upper bound.
   
   The NOT operator evaluates to the complement of an interval.  If Ia is 
   a finite inclusive interval [0, 30], then its complement is the pair of
   exclusive intervals (-Infinity, 0) and (30, +Infinity).
   
   The Unary Comparators "age IS NULL" and "age IS NOT NULL" seem to tell us 
   nothing, so they evaluate to (-Infinity, +Infinty).

   Calculating the intersections and unions of intervals requires us to 
   compare two values for an indexed column.  Implementing this would be 
   easy if all indexed columns were numbers, but in fact we also have 
   at least strings and dates to deal with.  JavaScript has a funny answer 
   when you compare strings and dates to Infinity:
      ("a" < Infinity) == false
      (Date() < Infinity) == false 
   isFinite() also cannot be used with strings and dates.

   For any two values encoded as data in buffers, NdbSqlUtil can compare them.
*/

"use strict";

var adapter            = require(path.join(build_dir, "ndb_adapter.node")),
    NdbScanFilter      = adapter.ndb.ndbapi.NdbScanFilter,
    IndexBound         = adapter.ndb.impl.IndexBound,
    udebug             = unified_debug.getLogger("IndexBounds.js");

function blah() {
  console.log("BLAH");
  console.log.apply(null, arguments);
  process.exit();
}

function isBoolean(x) {
  return (typeof x === 'boolean');
}

/* An EncodedValue holds a value that has been encoded in a buffer for 
   a particular column
*/
function EncodedValue(column, buffer, size) {
  this.column = column;
  this.buffer = buffer;
  this.size = size;
}

EncodedValue.prototype = {
  isEncodedValue : true;
};


function Point(value, inclusive) {
  this.value     = value;
  this.inclusive = inclusive;
}

Point.prototype.asString = function(close) {
  var s = "";
  if(close) {
    s += this.value;
    s += (this.inclusive ? "]" : ")");
  }
  else {
    s += (this.inclusive ? "[" : "(");
    s += this.value;
  }
};

/* Compare two points.
   Returns:
     -1 if this point's value is less than the other's
     +1 if this point's value is greater than the other's
     true if the values are equal and both points are inclusive
     false if the values are equal and either point is exclusive
   Caller is encouraged to test the return value with isBoolean().
*/
Point.prototype.compare = function(that) {
  var cmp;

  /* First compare to infinity */
  if(this.value == -Infinity || that.value == Infinity) {
    return -1;
  }

  if(this.value == Infinity  || that.value == -Infinity) {
    return 1;
  }

  if(typepof this.value === 'object' && this.isEncodedValue) {
    /* Compare Encoded Values */
    cmp = 0; // cmp = xxx.compare( ... )
  }
  else {
    /* Compare JavaScript values */
    if(this.value == that.value) cmp = 0;
    else if (this.value < that.value) cmp = -1;
    else cmp = 1;
  }

  if(cmp === 0) {
    return (this.inclusive && that.inclusive);
  }
  else {
    return cmp;
  }
};

/* complement flips Point between inclusive and exclusive
   Used in complementing number lines.
   e.g. the complement of [4,10] is (-Inf, 4) and (10, Inf)
*/
Point.prototype.complement = function() {
   this.inclusive = ! this.inclusive;
};


var negInf = new Point(-Infinity, false);
var posInf = new Point(Infinity, false);


/* A Segment is created from two points on the line
*/
function Segment(point1, point2) {
  if(point1.compare(point2) === -1) {
    this.low = point1;
    this.high = point2;
  }
  else {
    this.low = point2;
    this.high = point1;
  }
}

Segment.prototype.toString = function() {
  var s = "";
  s += this.low.asString(0) + "," + this.high.asString(1);
  return s;
}

Segment.prototype.contains = function(point) {
  var hi = this.high.compare(point);
  var lo = this.low.compare(point);
  return(   (lo === true) || (lo === -1) 
         && (hi === true) || (hi === 1));
}

Segment.prototype.intersects = function(that) {
  return (this.contains(that.low) || this.contains(that.high));
}

/* compare() returns 0 if segments intersect. 
   otherwise like Point.compare()
*/
Segment.prototype.compare = function(that) {
  var r = 0; 
  if(! this.intersects(that)) {
    r = this.low.compare(that.low);
  }
  return r;
}

/* intersection: greatest lower bound & least upper bound 
*/
Segment.prototype.intersection = function(that) {
  var s = null;
  var lp, hp;
  if(this.intersects(that)) {
    lp = (this.low.compare(that.low) == 1) ? this.low : that.low;
    hp = (this.high.compare(that.high) == -1) ? this.high : that.high;
    s = new Segment(lp, hp);
  }
  return s;
}

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
}

/* Process an inclusive BETWEEN operator 
*/
function createSegmentBetween(a, b) {
  var p1 = new Point(a, true);
  var p2 = new Point(b, true);
  return new Segment(p1, p2);
}



/* A number line represents a set of line segments on a key space
   stretching from -Infinity to +Infinity
   
   The line is stored as an ordered list of transition points. 
 
   The constructor "new NumberLine()" returns the full line from
   -Infinity +Infinity.
*/

function NumberLine() {
  this.transitions = [NegInf, posInf];
} 

NumberLine.prototype.setEmpty = function() {
  this.transitions = [];
  return this;
};

NumberLine.prototype.isEmpty = function() {
  return (this.transitions.length == 0);
};

NumberLine.prototype.complement = function() {
  var i;
  if(this.transitions[0].value == -Infinity) {
    this.transitions.unshift();
  }
  else {
    this.transitions.shift(negInf);
  }
  for(i = 1; i < this.transitions.length; i ++) {
    this.transitions[i].complement();
  }
  return this;
};

NumberLine.prototype.upperBound = function() {
  return (this.isEmpty() ? negInf : this.transitions[this.transitions.length - 1]);
};

NumberLine.prototype.lowerBound = function() {
  return (this.isEmpty() ? posInf : this.transitions[0]);
};

NumberLine.prototype.toString = function() {
  var i, close;  
  var list = this.transitions;
  var str = "";
  for(i = 0 ; i < list.length ; i ++) {
    if(i) str += ",";
    str += list[i].asString(i % 2);
  }
  return str;
}

/* A NumberLineIterator can iterate over the segments of a NumberLine 
*/
function NumberLineIterator(numberLine) {
  this.line = numberLine;
  this.list = numberLine.transitions;
  this.length = list.length / 2;
  this.n = 0;
}

NumberLineIterator.prototype.next = function() {
  var s = null;
  if(this.n < this.list.length) {
    s = new Segment(this.list[this.n], this.list[this.n+1]);
    this.n += 2;
  }
  return s;    
}

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
}


NumberLine.prototype.getIterator = function() { 
  return new NumberLineIterator(this);
}

/* A NumberLine intersects segment S if any of its segments 
   intersects S.
*/
NumberLine.prototype.intersects = function(segment) {
  var i = this.getIterator(); 
  var s;
  while(s = i.next()) {
    if(segment.intersects(s)) {
      return i.getSplicePoint();   //!!!!!!
    }
  }
  return false;
}

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
    var i = this.getIterator();
    var s, sp;
    while(s = i.next()) {
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
}

// With this much we can do union of Line with non-intersecting Segment.
// To do union of line and intersecting Segment we need to sort and span.
// (coalesce?) 

function createNumberLineFromSegment(segment) {
  var line = new NumberLine();
  if(segment.low !== -Infinity) {
    line.list.push(segment.low);
    line.hasNegInf = false;
  }
  if(segment.high !== Infinity) {
    line.list.push(segment.high);
  }
  return line;
}

/* Create a segment for a comparison expression */
function createNumberLine(operator, value) {
  var lp = new Point(-Infinity, false);
  var hp = new Point(Infinity, false);
  var s;
  var l = null;

  if(operator < COND_EQ) {
    switch(operator) {
      case COND_LE:
        hp.inclusive = true;
        /* fall through */
      case COND_LT:
        hp.value = value;
        break;

      case COND_GE:
        lp.inclusive = true;
        /* fall through */
      case COND_GT:
        lp.value = value;
        break;
    
    }
    s = new Segment(lp, hp);
    return createNumberLineFromSegment(s);
  }

  if(operator < COND_LIKE) { /* EQ and NE */
    lp.value = value;
    lp.inclusive = (operator === COND_EQ);
    l = new LineNumber();
    l.list.push(lp);
    l.list.push(lp);  /* Push it twice */
    l.hasNegInf = (operator === COND_NE);
    return l;
  }
}




/****************************************** IndexBoundVisitor ************
 *
 * Given a set of actual parameter values, 
 * calculate the bounding interval for a column
 */
function IndexBoundVisitor(column, params) {
  this.column = column;
  this.params = params;
}

/** Handle nodes QueryAnd, QueryOr */
IndexBoundVisitor.prototype.visitQueryNaryPredicate = function(node) {
    
  for(var i = 0 ; i < node.predicates.length ; i++) {
    node.predicates[i].visit(this);
  }
  
};

/** Handle nodes QueryEq, QueryNe, QueryLt, QueryLe, QueryGt, QueryGe */
IndexBoundVisitor.prototype.visitQueryComparator = function(node) {
  blah(node);
};

/** Handle node QueryNot */
IndexBoundVisitor.prototype.visitQueryUnaryPredicate = function(node) {
};

/** Handle node QueryBetween */
IndexBoundVisitor.prototype.visitQueryBetweenOperator = function(node) {
};

/** Handle nodes QueryIsNull, QueryIsNotNull */
IndexBoundVisitor.prototype.visitQueryUnaryOperator = function(node) {
};


function getBoundHelper(queryHander, keys) {
  


}

exports.getBoundHJelper = getBoundHelper;
 