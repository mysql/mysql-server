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

/* Evaluation of IndexBounds from a Query

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
   
   The Unary Comparators "age IS NULL" and "age IS NOT NULL" seem to tell us 
   nothing, so they evaluate to (-Infinity, +Infinty).

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
   value itself or not.  
   "inclusive" defaults to true for values other than Infinity and -Infinity
*/
function Endpoint(value, inclusive) {
  this.value = value;
  this.isFinite = (typeof value === 'number') ? isFinite(value) : true;
  
  if(typeof inclusive === 'boolean') 
    this.inclusive = inclusive;
  else 
    this.inclusive = this.isFinite;
}

Endpoint.prototype.isEndpoint = true;

Endpoint.prototype.asString = function(close) {
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
Endpoint.prototype.compare = function(that) {
  assert(that.isEndpoint);
  var cmp;

  /* First compare to infinity */
  if(this.value == -Infinity || that.value == Infinity) {
    return -1;
  }

  if(this.value == Infinity  || that.value == -Infinity) {
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
   e.g. the complement of [4,10] is (-Inf, 4) and (10, Inf)
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

Segment.prototype.toString = function() {
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
  
  return (this.contains(that.low) || this.contains(that.high));
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
  var lp, hp;
  assert(that.isSegment);
  if(this.intersects(that)) {
    lp = (this.low.compare(that.low) == 1) ? this.low : that.low;
    hp = (this.high.compare(that.high) == -1) ? this.high : that.high;
    s = new Segment(lp, hp);
  }
  return s;
};

/* span of intersecting segments: least lower bound & greatest upper bound
*/
Segment.prototype.span = function(that) {
  var s = null;
  var lp, hp;
  assert(that.isSegment);
  if(this.intersects(that)) {
    lp = (this.low.compare(that.low) == -1) ? this.low : that.low;
    hp = (this.high.compare(that.high) == 1) ? this.high : that.high;
    s = new Segment(lp, hp);
  }
  return s;
};


//////// NumberLine                 /////////////////

/* A number line represents a set of line segments on a key space
   stretching from -Infinity to +Infinity
   
   The line is stored as an ordered list of transition points. 
   
   The segments on the line are from P0 to P1, P2 to P3, etc.
 
   The constructor "new NumberLine()" returns the full line from
   -Infinity +Infinity.
*/

function NumberLine() {
  this.transitions = [negInf, posInf];
} 

NumberLine.prototype.isNumberLine = true;

NumberLine.prototype.setEmpty = function() {
  this.transitions = [];
  return this;
};

NumberLine.prototype.isEmpty = function() {
  return (this.transitions.length == 0);
};

NumberLine.prototype.upperBound = function() {
  if(this.isEmpty()) return negInf;
  return this.transitions[this.transitions.length - 1];
};

NumberLine.prototype.lowerBound = function() {
  if(this.isEmpty()) return posInf;
  return this.transitions[0];
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


/* A NumberLine intersects segment S if any of its segments intersects S.
   Returns false if no segment intersects S.
    
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
};

// With this much we can do union of Line with non-intersecting Segment.
// To do union of line and intersecting Segment we need to sort and span.
// (coalesce?) 

function createNumberLineFromSegment(segment) {
  var line = new NumberLine();
  line.transitions[0] = segment.low;
  line.transitions[1] = segment.high;
  return line;
}

/* Create a segment for a comparison expression */
function createNumberLine(operator, value) {
  var lp = new Endpoint(-Infinity, false);
  var hp = new Endpoint(Infinity, false);
  var s;
  var l = null;

//  if(operator < COND_EQ) {
//    switch(operator) {
//      case COND_LE:
//        hp.inclusive = true;
//        /* fall through */
//      case COND_LT:
//        hp.value = value;
//        break;
//
//      case COND_GE:
//        lp.inclusive = true;
//        /* fall through */
//      case COND_GT:
//        lp.value = value;
//        break;
//    
//    }
//    s = new Segment(lp, hp);
//    return createNumberLineFromSegment(s);
//  }
//
//  if(operator < COND_LIKE) { /* EQ and NE */
//    lp.value = value;
//    lp.inclusive = (operator === COND_EQ);
//    l = new LineNumber();
//    l.list.push(lp);
//    l.list.push(lp);  /* Push it twice */
//    l.hasNegInf = (operator === COND_NE);
//    return l;
//  }
}


/* Process an inclusive BETWEEN operator 
*/
function createSegmentBetween(a, b) {
  var p1 = new Endpoint(a);
  var p2 = new Endpoint(b);
  return new Segment(p1, p2);
}


/* Certain data types imply a set of bounds
*/
function getBoundingSegmentForDataType(columnMetadata) {
  var lowBound = -Infinity;
  var highBound = Infinity;

  if(columnMetadata.isUnsigned) lowBound = 0;

  if(columnMetadata.isIntegral) {
    if(columnMetadata.isUnsigned) {
      switch(columnMetadata.intSize) {
        case 1:   highBound = 255;        break;
        case 2:   highBound = 65535;      break;
        case 3:   highBound = 16777215;   break;
        case 4:   highBound = 4294967295; break;
        default:  break;
      }
    }
    else {
      switch(columnMetadata.intSize) {
        case 1:   highBound = 127;        lowBound = -128;        break;
        case 2:   highBound = 32767;      lowBound = -32768;      break;
        case 3:   highBound = 8338607;    lowBound = -8338608;    break;
        case 4:   highBound = 2147483647; lowBound = -2147483648; break;
        default:  break;
      }
    }
  }

  return new Segment(new Endpoint(lowBound, highBound));
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
  var i;  
  for(i = 0 ; i < node.predicates.length ; i++) {
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
 