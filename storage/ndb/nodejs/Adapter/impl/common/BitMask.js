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

/* BitMask is implemented as an array.
   Each array element holds 24 bits.
   Inside V8, array elements are SMIs.
*/
var radix = 24;

function BitMask(size) {
  this._mask = [];
  if(typeof size === 'number') {
    this._grow(size);
  }
  else {
    this._grow(radix);
  }
}

function greater(x, y) {
  return x > y ? x : y;
}

BitMask.prototype.inspect = function() {
  var str = "";
  this._mask.forEach(function(value) { str += value.toString(2); });
  return str;
};

BitMask.prototype._grow = function(size) {
  this.size = this._mask.length * radix;  // round up
  size -= this.size;
  while(size > 0) { 
    this._mask.push(0);
    this.size += radix;
    size -= radix;  
  }
};

BitMask.prototype.set = function(bit) {
  if(bit >= this.size) {
    this._grow(bit);
  }
  var offset = bit % radix;
  var loc = (bit - offset) / radix;
  this._mask[loc] |= Math.pow(2, offset);
};

BitMask.prototype.bitIsSet = function(bit) {
  if(bit >= this.size) {
    this._grow(bit);
  }
  var offset = bit % radix;
  var loc = (bit - offset) / radix;
  return this._mask[loc] & Math.pow(2, offset) ? true : false;
};

BitMask.prototype._getPart = function(n) {
  var p = 0;
  if(n < this._mask.length) {
    p = this._mask[n];
  }
  return p;
};

BitMask.prototype._binaryOp = function(op, that) {
  var m, i, sz;
  sz = greater(this.size, that.size);
  m = new BitMask(sz); 
  for(i = 0 ; i < m._mask.length ; i++) {
    switch(op) {
      case 1:     // AND
        m._mask[i] = this._getPart(i) & that._getPart(i);
        break;
      case 2:     // OR
        m._mask[i] = this._getPart(i) | that._getPart(i);
        break;
      case 3:     // XOR
        m._mask[i] = this._getPart(i) ^ that._getPart(i);
        break;
    }
  }
  return m;
};

BitMask.prototype.and = function(that) {
  return this._binaryOp(1, that);
};

BitMask.prototype.or = function(that) {
  return this._binaryOp(2, that);
};

BitMask.prototype.xor = function(that) {
  return this._binaryOp(3, that);
};

BitMask.prototype.orWith = function(that) {
  var mask = this.or(that);
  this.size = mask.size;
  this._mask = mask._mask;
  return this;
};

BitMask.prototype.isNonZero = function() {
  var i;
  for(i = 0 ; i < this._mask.length ; i++) {
    if (this._mask[i]) return true;
  }
  return false;
};

BitMask.prototype.isEqualTo = function(that) {
  var i, len;
  len = greater(this._mask.length, that._mask.length);
  for(i = 0 ; i < len ; i++) {
    if(this._getPart(i) !== that._getPart(i)) {
      return false;
    }
  }
  return true;
};

BitMask.prototype.toArray = function() {
  var array, n, offset;
  array = [];
  offset = 0;

  function maskToIntArray(mask) {
    var v, i;
    i = 0;
    while(mask != 0) {
      v = Math.pow(2,i);
      if(mask & v) {
        mask ^= v;
        array.push(i + offset);
      }
      i++;
    }
  }

  for(n = 0 ; n < this._mask.length ; n++) {
    maskToIntArray(this._mask[n]);
    offset += radix;
  }

  return array;
};

module.exports = BitMask;

