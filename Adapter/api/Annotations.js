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

"use strict";

var fieldmapping = require("./FieldMapping.js"),
    tablemapping = require("./TableMapping.js");

/** Annotations constructor
*/
function Annotations() {
  this.strictValue = undefined;
  this.mapAllTablesValue = true;
  this.mappings = [];
}


/** In strict mode, all parameters of mapping functions must be valid */
Annotations.prototype.strict = function(value) {
  if (value === true || value === false) {
    this.strictValue = value;
  }
};

/** Map all tables */
Annotations.prototype.mapAllTables = function(value) {
  if (value === true || value === false) {
    this.mapAllTablesValue = value;
  }
};

/** Map tables */
Annotations.prototype.mapClass = function(ctor, mapping) {
  var x, i, validity;
  if (typeof(ctor) !== 'function') {
    throw new Error('mapClass takes a constructor function and a mapping.');
  }
  if(this.strictValue) {
    validity = tablemapping.isStrictlyValidTableMapping(mapping);
    if(validity !== true) {
      throw new Error('mapClass(): ' + validity);
    }
  }
  if (typeof(ctor.prototype.mynode) === 'undefined') {
    ctor.prototype.mynode = {};
  }
  ctor.prototype.mynode.mapping = mapping;
  ctor.prototype.mynode.constructor = ctor;
  this.mappings.push(ctor.prototype.mynode);
};


Annotations.prototype.newTableMapping = function(tableName) {
  if(! tableName) {
    throw new Error("Annotations.newTableMapping(): tableName required.");
  }
  return new tablemapping.TableMapping(tableName);
};


Annotations.prototype.newFieldMapping = function(fieldName, columnName) {
  return new fieldmapping.FieldMapping(fieldName, columnName);
};

exports.Annotations = Annotations;
