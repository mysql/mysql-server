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

'use strict';

var mynode = require(".."),
    random = require("../samples/ndb_loader/lib/RandomData"),
    udebug = require("../Adapter/api/unified_debug"),
    stats  = require("../Adapter/api/stats");

function usage() {
  var msg = "" +
  "Usage:  node insert [options]\n" +
  "   -d    :  Enable debug output\n" +
  "   -l    :  Test latency\n" +
  "   -t    :  Test throughput\n" +
  "   -f    :  Test find()\n" +
  "   -n    :  Use ndb adapter (default)\n" +
  "   -m    :  Use mysql adapter\n"
  ;
  console.log(msg);
  process.exit(1);
}


function parse_command_line(options) {
  var i, len, val;
  len = process.argv.length;
  for(i = 2 ; i < len; i++) {
    val = process.argv[i];
    switch(val) {
      case '-d':
        udebug.level_debug();
        break;
      case '-t':
        options.mode = "throughput";
        break;
      case '-l':
        options.mode = "latency";
        break;
      case '-f':
        options.mode = "find";
        break;
      case '-n':
        options.adapter = "ndb";
        break;
      case '-m':
        options.adapter = "mysql";
        break;
      default:
        usage();
    }  
  }
}

function Row() {
}

function getIntervalTimer() {
  var time = Date.now();
  console.log("Interval timer initialized.");  

  return function interval(description) {
    var current = Date.now();
    var result = current - time;
    console.log(description, " Interval: ", result, "ms.");
    time = current;
    return result;
  }
}

function doThroughputTest(error, session, batchSize, timeInterval) {

  function onDone(error) {
    stats.peek();
    var r = timeInterval("Batch completed.");
    console.log("Avg. Latency", r / batchSize, "ms. per operation");
    console.log("Throughput", (batchSize * 10000) / r, " rows per sec.");
  }

  function onRowInserted(error, mystery) {
    if(error) { console.log(error, mystery); }
  }

  function doBatchInsert(rdg) {
    timeInterval("Starting batch.");
    var batch = session.createBatch();
    var i = 0;
    var row;
    for (; i < batchSize ; i++) {
      row = rdg.newRow();
      batch.persist("a", row, onRowInserted);
    }
    batch.execute(onDone);
  }

  function onTable(err, metadata) {
    var generator = new random.RandomRowGenerator(metadata);
    doBatchInsert(generator);   
  }

  /* doBatchInsert starts here */
  session.getTableMetadata("jscrund", "a", onTable);
}


function doSingleInserts(error, session, count, timeInterval) {
  var n = 0;
  var generator;
  
  function onRowInserted(error) {
    n += 1;
    var row = generator.newRow();
    var r;
    if(n >= count) {
      r = timeInterval("Inserts completed.");
      console.log("Avg. Latency", r / count, "ms. per operation");
      console.log("Throughput", (count * 10000) / r, " rows per sec.");
    }
    else {
      session.persist("a", row, onRowInserted);
    }
  }

  function onTable(err, metadata) { 
    generator = new random.RandomRowGenerator(metadata);
    var row = generator.newRow();
    timeInterval("Starting Inserts.");
    session.persist("a", row, onRowInserted);
  }
    
  /* doSingleInserts starts here */
  session.getTableMetadata("jscrund", "a", onTable);
}


function doFindTest(err, session, count, timeInterval) {
  var n = 0;
  
  function onRowFound(error, data) {
    n += 1;
    var key = Math.floor(Math.random() * count);
    if(n >= count) {
      var r = timeInterval("Find completed.");
      console.log("Avg. Latency", r / count, "ms. per operation");
      console.log("Throughput", (count * 10000) / r, " rows per sec.");
    }
    else {
      session.find("a", key, onRowFound);
    }
  }
  
  function onTable(err, metadata) {
    var key = Math.floor(Math.random() * count);
    timeInterval("Starting find.");
    session.find("a", key, onRowFound);
  }
  
  /* doFindTest() starts here */
  session.getTableMetadata("jscrund", "a", onTable);
}

  
function main() {
  var options = {  /* Default options: */
    "adapter" : "ndb"
  };
  parse_command_line(options);
  var tm = new mynode.TableMapping("a").applyToClass(Row);
  var intervalTimer = getIntervalTimer();
  properties.database = "jscrund";
  properties.mysql_user = "root";

  switch(options.mode) {
    case "throughput":
      mynode.openSession(properties, null, doThroughputTest, 4000, intervalTimer);
      break;
    case "latency":
      mynode.openSession(properties, null, doSingleInserts, 4000, intervalTimer);
      break;
    case "find":
      mynode.openSession(properties, null, doFindTest, 4000, intervalTimer);
      break;
    default:
      usage();
  }
}

main();

