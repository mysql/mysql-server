/*
 Copyright (c) 2013, 2022, Oracle and/or its affiliates.
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2.0,
 as published by the Free Software Foundation.

 This program is also distributed with certain software (including
 but not limited to OpenSSL) that is licensed under separate terms,
 as designated in a particular file or component or in included license
 documentation.  The authors of MySQL hereby grant you an additional
 permission to link the program and your derivative works with the
 separately licensed software that they have included with MySQL.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License, version 2.0, for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

"use strict";

var util = require("util"),
    assert = require("assert"),
    jones = require("database-jones"),
    conf = require("./path_config"),
    adapter = require(conf.binary).ndb,
    udebug = unified_debug.getLogger("NdbProjection.js"),
    stats = { "rootProjectionsCreated" : 0, "rewrittenToScan" : 0 };

require(jones.api.stats).register(stats, "spi","ndb","NdbProjection");

function blah() {
  console.log("BLAH");
  console.log.apply(null, arguments);
  console.trace();
  process.exit();
}

function mockKeys(columnNames) {
  var mock_keys = {};
  columnNames.forEach(function(field) {
    mock_keys[field] = "_";
  });
  return mock_keys;
}

function buildJoinTableResultRecord(dbTableHandler) {
  if(! dbTableHandler.resultRecord) {
    dbTableHandler.resultRecord = adapter.impl.DBDictionary.getRecordForMapping(
        dbTableHandler.dbTable,
        dbTableHandler.dbTable.per_table_ndb,
        dbTableHandler.getNumberOfColumns(),
        dbTableHandler.getAllColumnMetadata()
    );
  }
}

function NdbProjection(tableHandler, indexHandler, previous, parent) {
  if(previous) {
    this.root         = previous.root;
    this.serial       = previous.serial + 1;
    this.parent       = parent || previous;   // parent in tree structure
    previous.next     = this;                 // next in linked list structure
    this.root.size++;                         // number of in list
  } else { 
    this.root         = this;
    this.serial       = 0;
    this.size         = 1;
  }
  this.error          = null;
  this.hasScan        = null;
  this.tableHandler   = tableHandler;
  this.rowRecord      = tableHandler.resultRecord;
  this.indexHandler   = indexHandler;
  this.keyRecord      = indexHandler.dbIndex.record;
  this.isPrimaryKey   = indexHandler.dbIndex.isPrimaryKey;
  this.isUniqueKey    = indexHandler.dbIndex.isUnique;
}


function ndbRootProjection(sector, indexHandler) {
  var p;

  udebug.log("Root", sector);
  stats.rootProjectionsCreated++;

  p = new NdbProjection(sector.tableHandler, indexHandler);
  p.keyFields    = sector.keyFieldNames;
  p.joinTo       = null;
  p.relatedField = sector.parentFieldMapping;  // should be unused!
  p.hasScan      = ! (p.isPrimaryKey || p.isUniqueKey);
  return p;
}

function ndbProjectionToJoinTable(sector, previousProjection, parentProjection) {
  var mock_keys, indexHandler, p;
  udebug.log("ToJoinTable:", sector);

  mock_keys = mockKeys(sector.parentFieldMapping.thisForeignKey.columnNames);
  indexHandler = sector.joinTableHandler.getOrderedIndexHandler(mock_keys);

  buildJoinTableResultRecord(sector.joinTableHandler);

  p = new NdbProjection(sector.joinTableHandler, indexHandler,
                        previousProjection, parentProjection);
  p.keyFields    = sector.parentFieldMapping.thisForeignKey.columnNames;
  p.joinTo       = sector.parentFieldMapping.thisForeignKey.targetColumnNames;
  p.relatedField = null;   // No result fields come from the join table
  p.root.hasScan = true;
  return p;
}

function ndbProjectionFromJoinTable(sector, parentProjection) {
  var mock_keys, indexHandler, p;
  udebug.log("FromJoinTable:", sector);

  mock_keys = mockKeys(sector.parentFieldMapping.otherForeignKey.targetColumnNames);
  indexHandler = sector.tableHandler.getIndexHandler(mock_keys);

  p = new NdbProjection(sector.tableHandler, indexHandler, parentProjection);
  p.keyFields    = sector.parentFieldMapping.otherForeignKey.targetColumnNames;
  p.joinTo       = sector.parentFieldMapping.otherForeignKey.columnNames;
  p.relatedField = sector.parentFieldMapping;
  return p;
}

function createNdbProjection(sectors, projections, id) {
  var sector, previousProjection, parentProjection, indexHandler, p;

  sector = sectors[id];
  previousProjection = projections[id-1];
  parentProjection = projections[sectors[id].parentSectorIndex];
  assert(parentProjection);

  if(sector.joinTableHandler) {
    p = ndbProjectionToJoinTable(sector, previousProjection, parentProjection);
    return ndbProjectionFromJoinTable(sector, p);
  }

  udebug.log(sector);
  indexHandler = sector.tableHandler.getIndexHandler(mockKeys(sector.thisJoinColumns));
  p = new NdbProjection(sector.tableHandler, indexHandler,
                        previousProjection, parentProjection);
  p.keyFields    = sector.thisJoinColumns;
  p.joinTo       = sector.otherJoinColumns;
  p.relatedField = sector.parentFieldMapping;

  if(! (p.isPrimaryKey || p.isUniqueKey)) {
    p.root.hasScan = true;
  }
  return p;
}

/* If the root operation is a find, but some child operation is a scan,
   NdbQueryBuilder.cpp says "Scan with root lookup operation has not been
   implemented" and returns QRY_WRONG_OPERATION_TYPE error 4820. 
   We have to work around this now by rewriting the root to use a scan.

   NOTE: The server uses a different strategy here.  Divides the tree after
   the last consecutive lookup then runs all lookups.

   TODO: NdbProjection should detect whether SPJ can execute the whole query.
   If not, it should divide the query into parts that can be executed
   independently and return this to NdbOperation, so that NdbOperation can
   itself manage a join.
*/
NdbProjection.prototype.rewriteAsScan = function(sector) {
  var mock_keys = mockKeys(sector.keyFieldNames);
  this.indexHandler = this.tableHandler.getOrderedIndexHandler(mock_keys);
  if(this.indexHandler) {
    this.isPrimaryKey = false;
    this.isUniqueKey = false;
    stats.rewrittenToScan++;
  } else {
    this.error = new Error("Could not rewrite NdbProjection to use scan");
  }
};


function initializeProjection(sectors, indexHandler) {
  var projections, root, i;
  projections = [];
  projections[0] = root = ndbRootProjection(sectors[0], indexHandler);

  for (i = 1 ; i < sectors.length ; i++) {
    projections[i] = createNdbProjection(sectors, projections, i);
  }

  if(root.hasScan && (root.isPrimaryKey || root.isUniqueKey))
  {
    udebug.log("Rewriting to scan");
    root.rewriteAsScan(sectors[0]);
  }

  return root;
}

exports.initialize = initializeProjection;

