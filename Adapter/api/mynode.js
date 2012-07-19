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

var spi = require("../impl/SPI.js");


function Annotations() {
  this.strictValue = undefined;
  this.mapAllTablesValue = true;
  this.mappings = [];
}

exports.Annotations = function() {
  return new Annotations();
};

/** A Mapping holds the mapping for a single class */
Mapping = function(proto, mapping) {
  this.proto = proto;
  this.mapping = mapping;
};

/** In strict mode, all parameters of mapping functions must be valid */
Annotations.prototype.strict = function(value) {
  if (value == true || value == false) {
    this.strictValue = value;
  }
};

/** Map all tables */
Annotations.prototype.mapAllTables = function(value) {
  if (value == true || value == false) {
    this.mapAllTablesValue = value;
  }
};

/** Map tables */
Annotations.prototype.mapClass = function(proto, mapping) {
  if (this.strictValue != false) {
    for (x in mapping) {
      switch (x) {
      case 'table':
      case 'schema':
      case 'database':
      case 'autoIncrementBatchSize':
      case 'mapAllColumns':
        break;
      case 'field':
      case 'fields':
        // look inside the fields
        var fields = mapping[x];
        if (typeof(fields) != 'array') {
          fields = [fields];
        }
        fields.forEach(function(field, index, mapping) {
          for (x in field) {
            switch(x) {
            case 'name':
            case 'nullValue':
            case 'column':
            case 'notPersistent':
            case 'converter':
              break;
            default:
              var err = new Error('bad property ' + x);
              throw err;
            }
          }
        });
        break;
      default:
        var err = new Error('bad property ' + x);
        throw err;
      }
    }
  }
  this.mappings.push(new Mapping(proto, mapping));
};

function spi_connect_sync(properties) {
  var db = spi.getDBServiceProvider(properties.implementation);
  return db.connectSync(properties);  
}

exports.ConnectionProperties = function(name) {
  var sp = spi.getDBServiceProvider(name);
  return sp.getDefaultConnectionProperties();
};

