/*
 Copyright (c) 2013, 2016, Oracle and/or its affiliates. All rights reserved.
 
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

'use strict';

/*global JSCRUND */
global.JSCRUND = {};

// Modules:
JSCRUND.mynode = require("database-jones");
JSCRUND.unified_debug = require("unified_debug");
JSCRUND.udebug = JSCRUND.unified_debug.getLogger("jscrund.js");
JSCRUND.stats  = require(JSCRUND.mynode.api.stats);

// Backends:
JSCRUND.mysqljs = require('./jscrund_mysqljs');

JSCRUND.errors  = [];

var DEBUG, DETAIL;
var path = require("path"),
    fs = require("fs");


// webkit-devtools-agent allows you to profile the process from a Chrome browser
try {
 require('webkit-devtools-agent');
} catch(e) { };

function usage() {
  var msg = "" +
  "Usage:  node jscrund [options]\n" +
  "   --help:\n" +
  "   -h      :  Print help (this message)\n" +
  "   -A      :  Run \"A\" tests (default)\n" +
  "   -B      :  Run \"B\" tests\n" +
  "   --varchar=size\n" +
  "           :  Specify varchar size in B tests (default 10)\n" +
  "   --adapter=ndb\n" +
  "   -n      :  Use ndb adapter (default)\n" +
  "   --adapter=mysql\n" +
  "   -m      :  Use mysql adapter\n" +
  "   --spi   :  Run tests using DBServiceProvider SPI \n" +
  "   --adapter=sql\n" +
  "   -f      :  Use felix sql driver (not mysql-js api)\n" +
  "   --adapter=null\n" +
  "           :  Use null driver\n" +
  "   --log   :  Write log file\n" +
  "   --detail:  Enable detail debug output\n" +
  "   --debug :\n" +
  "   -d      :  Enable debug output\n" +
  "   -df=file:  Enable debug output only from source file <file>\n" +
  "   -i <n>  :  Specify number of iterations per test (default 4000)\n" +
  "   --delay=<m>,<n>\n" +
  "           :  Delay <m> seconds after first iteration and <n> seconds before exiting\n" + 
  "   --modes :\n" +
  "   --mode  :  Specify modes to run (default indy,each,bulk)\n" +
  "   --tests :\n" +
  "   --test  :  Specify tests to run (default persist,find,remove)\n" +
  "   -r <n>  :  Repeat tests #n times (default 1, n<0: forever)\n" +
  "   --trace :\n" +
  "   -t      :  Enable trace output\n" +
  "   --set prop=value: set connection property prop to value\n" +
  "   -E <name> \n" +
  "   --deployment=<name>\n: use deplyment <name> from jones_deployments.js\n"
  ;
  console.log(msg);
  process.exit(1);
}

//handle command line arguments
function parse_command_line(options) {
  var i, val, values, pair, m, n;
  for(i = 2; i < process.argv.length ; i++) {
    val = process.argv[i];
    switch (val) {
    case '--help':
    case '-h':
      options.exit = true;
      break;
    case '-A':
      options.doClass = "A";
      break;
    case '-B':
      options.doClass = "B";
      break;
    case '-n':
      options.adapter = "ndb";
      break;
    case '-m':
      options.adapter = "mysql";
      break;
    case '-f':
      options.adapter = "sql";
      break;
    case '-i':
      var iterList = process.argv[++i];
      options.iterations = iterList.split('\,');
      for(m = 0 ; m < options.iterations.length; ++m) {
        n = options.iterations[m]
        if (isNaN(n)) {
          console.log('iterations value ',n,' is not allowed');
          options.exit = true;
        }
      }
      break;
    case '-r':
      options.nRuns = parseInt(process.argv[++i]);
      if (isNaN(options.nRuns)) {
        console.log('runs value is not allowed:', process.argv[i]);
        options.exit = true;
      }
      break;
    case '-E':
      options.deployment = process.argv[++i];
      break;
    case '--stats':
      options.stats = true;
      break;
    case '--debug':
    case '-d':
      JSCRUND.unified_debug.on();
      JSCRUND.unified_debug.level_debug();
      options.printStackTraces = true;
      break;
    case '--detail':
      JSCRUND.unified_debug.on();
      JSCRUND.unified_debug.level_detail();
      options.printStackTraces = true;
      break;
    case '--trace':
    case '-t':
      options.printStackTraces = true;
      break;
    case '--log':
      options.log = true;
      break;
    case '--spi':
      options.spi = true;
      break;
    case '--set':
      i++;  // next argument
      pair = process.argv[i].split('=');
      if(pair.length === 2) {
        if(isFinite(parseInt(pair[1]))) {
          pair[1] = parseInt(pair[1])
        }
        if(DEBUG) JSCRUND.udebug.log("Setting global:", pair[0], "=", pair[1]);
        options.setProp[pair[0]] = pair[1];
      }
      else {
        console.log("Invalid --set option " + process.argv[i]);
        options.exit = true;
      }
      break;
    default:
      values = val.split('=');
      if (values.length === 2) {
        switch (values[0]) {
        case '--count':
          options.count = values[1];
          break;
        case '--adapter':
          options.adapter = values[1];
          break;
        case '--mode':
        case '--modes':
          options.modes = values[1];
          options.modeNames = options.modes.split('\,');
          // validate mode names
          for (m = 0; m < options.modeNames.length; ++m) {
            switch (options.modeNames[m]) {
            case 'indy':
            case 'each':
            case 'bulk':
              break;
            default:
              console.log('Invalid mode ' + options.modeNames[m]);
              options.exit = true;
            }
          }
          break;
        case '--test':
        case '--tests':
          options.tests = values[1];
          options.testNames = options.tests.split('\,');
          // validate test names
          for (var t = 0; t < options.testNames.length; ++t) {
            switch (options.testNames[t]) {
            case 'persist':
            case 'find':
            case 'remove':
            case 'setVarchar':
            case 'clearVarchar':
              break;
            default:
              console.log('Invalid test ' + options.testNames[t]);
              options.exit = true;
            }
          }
          break;
        case '-df':
          unified_debug.on();
          unified_debug.set_file_level(values[1], 5);
          break;
        case '--delay':
          var delays = values[1].split(',');
          options.delay_pre = delays[0];
          options.delay_post = delays[1];
          break;
        case '--varchar':
          options.B_varchar_size = values[1];
          break;
        case '--deployment':
          options.deployment = values[1];
          break;
        default:
          console.log('Invalid option ' + val);
          options.exit = true;
        }
      } else {
        console.log('Invalid option ' + val);
        options.exit = true;
     }
    }
  }
}

/** Timer functions for crund. Multiple timers can be used simultaneously,
 * as long as each timer is created via new Timer().
 * start() starts the timer.
 * stop() stops the timer and writes results.
 * mode is the mode of operation (indy, each, or bulk)
 * operation is the operation (e.g. persist, find, delete)
 * numberOfIterations is the number of iterations of each operation
 */
function Timer() {
}

Timer.prototype.start = function(mode, operation, numberOfIterations) {
  //console.log('lib.Timer.start', mode, operation, 'iterations:', numberOfIterations);
  this.mode = mode;
  this.operation = operation;
  this.numberOfIterations = numberOfIterations;
  this.current = Date.now();
};

Timer.prototype.stop = function() {
  function pad(str, count, onRight) {
    while (str.length < count)
      str = (onRight ? str + ' ' : ' ' + str);
    return str;
  }
  function rpad(num, str) {
    return pad(str, num, true);
  }
  function lpad(num, str) {
    return pad(str, num, false);
  }
  this.interval = Date.now() - this.current;
  this.average = this.interval / this.numberOfIterations;
  var ops = Math.round(this.numberOfIterations * 1000 / this.interval);
  console.log(rpad(18, this.mode + ' ' + this.operation),
              '    time: ' + lpad(4, this.interval.toString()) + 'ms',
              '    avg latency: ' + lpad(4, this.average.toFixed(3)) + 'ms',
              '    ops/s: ' + lpad(4, ops.toString()));
};

/** Error reporter 
 */
function appendError(error) {
  JSCRUND.errors.push(error);
  if ((options.printStackTraces) && typeof(error.stack) !== 'undefined') {
    JSCRUND.errors.push(error.stack);
  }
};

function verifyObject(that) {
  for (var prop in this) {
    // only verify immediate properties, not inherited ones
    if (this.hasOwnProperty(prop)) {
      // only compare value, not type, since long numbers mapped to string
      //if (this[prop] !== that[prop])
      if (this[prop] != that[prop])
          appendError('Error: data mismatch for property '
                      + this.constructor.name + '.' + prop
                      + ' expected: ' + JSON.stringify(this[prop])
                      + ' actual: ' + JSON.stringify(that[prop]));
    }
  }
};

/** Constructor for domain object for A mapped to table a.
 */
function A() {
}

A.prototype.init = function(i) {
  this.id = i;
  this.cint = -i;
  this.clong = -i;
  this.cfloat = -i;
  this.cdouble = -i;
};

A.prototype.verify = verifyObject;

/** Constructor for domain object for B mapped to table b.
 */
function B() {
}

B.prototype.init = function(i) {
  this.id = i;
  this.cint = -i;
  this.clong = -i;
  this.cfloat = -i;
  this.cdouble = -i;
};

B.prototype.setVarchar = function(len) {
  this.cvarchar_def = ___;
};

B.prototype.verify = verifyObject;


/** Result Logging
 */
function currentDateString() {
  function zpad(s) {
    return (s.length == 1) ? "0" + s : s;
  }
  var d = new Date();
  var yy = d.getFullYear();
  var mm = zpad("" + (d.getMonth() + 1));
  var dd = zpad("" + d.getDate());
  var hh = zpad("" + d.getHours());
  var mn = zpad("" + d.getMinutes());
  var sc = zpad("" + d.getSeconds());
  return ("" + yy + mm + dd + "_" + hh + mn + sc);
};

function ResultLog(enabled) {
  this.enabled = enabled;
  if(enabled) {
    this.name = "log_" + currentDateString() + ".txt";
    this.fd = fs.openSync(this.name, 'a');
  } else {
    this.name = "[none]";
    this.message = "";
  }
}

ResultLog.prototype.write = function(message) {
  if(this.enabled) {
    var buffer = new Buffer(message);
    fs.writeSync(this.fd, buffer, 0, buffer.length);
  } else {
    this.message += message;
  }
};

ResultLog.prototype.close = function() {
  if(this.enabled) {
    fs.closeSync(this.fd);
  } else {
    console.log(this.message);
  }
};

/** Options are set up based on command line.
 * Properties are set up based on options.
 */
function main() {
  var config_file_exists = false;

  /* Default options: */
  var options = {
    'doClass' : 'A',
    'adapter' : 'ndb',
    'database': 'jscrund',
    'modes': 'indy,each,bulk',
    'tests': null,
    'iterations': [4000],
    'stats': false,
    'spi': false,
    'log': false,
    'nRuns': 1,
    'setProp' : {},
    'delay_pre' : 0,
    'delay_post' : 0,
    'B_varchar_size' : 10,
    'deployment' : 'test'
  };

  /* Options from config file */
  try {
    var config_file = require("./jscrund.config");
    config_file_exists = true;
    for(var i in config_file.options) {
      if(config_file.options.hasOwnProperty(i)) {
        options[i]  = config_file.options[i];
      }
    }
  }
  catch(e) {
    if (e.message.indexOf('Cannot find module') === -1) {
      console.log(e);
      console.log(e.name, 'reading jscrund.config:', e.message, '\nPlease correct this error and try again.\n');
      process.exit(0);
    }
  }

  /* Options from command line */
  parse_command_line(options);

  if (options.exit) {
    usage();
    process.exit(0);
  }

  /* Global udebug level; may have been set by options */
  DEBUG  = JSCRUND.udebug.is_debug();
  DETAIL = JSCRUND.udebug.is_detail();

  /* Create the string value for varchar tests */
  if(options.B_varchar_size > 0) {
    options.B_varchar_value = "";
    for(var i = 0 ; i < options.B_varchar_size ; i++) {
      options.B_varchar_value += String.fromCharCode(48 + (i % 64));
    }
  }

  /* Fetch the backend implementation */
  if(options.spi) {
    JSCRUND.spiAdapter = require('./jscrund_dbspi');
    JSCRUND.implementation = new JSCRUND.spiAdapter.implementation();
  } else if(options.adapter == 'sql') {
    JSCRUND.sqlAdapter = require('./jscrund_sql');
    JSCRUND.implementation = new JSCRUND.sqlAdapter.implementation();
  } else if(options.adapter == 'null') {
    JSCRUND.nullAdapter = require('./jscrund_null');
    JSCRUND.implementation = new JSCRUND.nullAdapter.implementation();
  } else {
    JSCRUND.implementation = new JSCRUND.mysqljs.implementation();
  }

  /* Get connection properties */
  var properties;
  if(typeof JSCRUND.implementation.getConnectionProperties === 'function') {
    properties = JSCRUND.implementation.getConnectionProperties();
  } else {
    properties = new JSCRUND.mynode.ConnectionProperties(options.adapter, options.deployment);
  }

  /* Then mix in properties from the command line */
  properties.database = options.database;
  for(i in options.setProp) {
    if(options.setProp.hasOwnProperty(i)) {
      properties[i] = options.setProp[i];
    }
  }

  /* Finally store the complete properties object in the options */
  options.properties = properties;

  /* Force GC is available if node is run with the --expose-gc option
  */
  options.use_gc = ( typeof global.gc === 'function' );

  var logFile = new ResultLog(options.log);
  new JSCRUND.mynode.TableMapping("a").applyToClass(A);
  new JSCRUND.mynode.TableMapping("b").applyToClass(B);
  options.annotations = [ A, B ];

  var generateAllParameters = function(numberOfParameters) {
    var result = [];
    for (var i = 0; i < numberOfParameters; ++i) {
      result[i] = generateParameters(i);
    }
    return result;
  };

  var generateParameters = function(i) {
    return {'key': generateKey(i), 'object': generateObject(i)};
  };

  var generateKey = function(i) {
    return i;
  };

  var generateObject = function(i) {
    var result;
    if(options.doClass == "A") {
      result = new A();
    } else if(options.doClass == "B") {
      result = new B();
    } else {
      assert(false);
    }
    result.init(i);
    return result;
  };


  // mainTestLoop (-r 10)
  //   batchSizeLoop (-i 1,10,100)
  //     ModeLoop      (--modes=indy,each)
  //       TestsLoop     (--tests=persist,remove)

  var runTests = function(options) {

    var timer = new Timer();

    var modeNames = options.modes.split('\,');    
    var modeNumber = 0;
    var mode;
    var modeName;

    var testNames;
    var testNumber = 0;
    var operation;
    var testName;

    var key;
    var object;

    var numberOfBatchLoops = options.iterations.length;
    var batchLoopNumber = 0;
    var numberOfIterations;
    var iteration;

    var nRun = 0;
    var nRuns = (options.nRuns < 0 ? Infinity : options.nRuns);
    var nReport = 0;

    var operationsDoneCallback;
    var testsDoneCallback;
    var resultStats = [];

    var parameters;

    /* Which tests to run */
    if(options.tests) {  // Explicit test names
      testNames = options.tests.split('\,');
    } else {
      switch(options.doClass) {
        case 'B':
          testNames = [ 'persist', 'setVarchar', 'find', 'clearVarchar', 'remove' ];
          break;
        case 'A':
        default:
          testNames = [ 'persist', 'find', 'remove' ];
          break;
      };
    }

    /** Recursively call the operation numberOfIterations times in autocommit mode
     * and then call the operationsDoneCallback
     */
    var indyOperationsLoop = function(err) {
      if(DETAIL) JSCRUND.udebug.log_detail('jscrund.indyOperationsLoop iteration:', iteration, 'err:', err);
      // check result
      if (err) {
        appendError(err);
      }
      // call implementation operation
      if (iteration < numberOfIterations) {
        operation.apply(JSCRUND.implementation, [parameters[iteration], indyOperationsLoop]);
        iteration++;
      } else {
        if(DETAIL) JSCRUND.udebug.log_detail('jscrund.indyOperationsLoop iteration:', iteration, 'complete.');
        timer.stop();
        resultStats.push({
          name: testName + "," + modeName,
          time: timer.interval
        });
        setImmediate(operationsDoneCallback);
      }
    };

    /** Call indyOperationsLoop for all of the tests in testNames
     */
    var indyTestsLoop = function() {
      testName = testNames[testNumber];
      operation = JSCRUND.implementation[testName];
      operationsDoneCallback = indyTestsLoop;
      if (testNumber < testNames.length) {
        if(options.use_gc) global.gc();  // Full GC between tests
        testNumber++;
        if(DETAIL) JSCRUND.udebug.log_detail('jscrund.indyTestsLoop', testNumber, 'of', testNames.length, ':', testName);
        iteration = 0;
        timer.start(modeName, testName, numberOfIterations);
        setImmediate(indyOperationsLoop, null);
      } else {
        // done with all indy tests
        // stop timer and report
        setImmediate(testsDoneCallback);
      }
    };

    /** Finish the each operations loop after commit.
     */
    var eachCommitDoneCallback = function(err) {
      // check result
      if (err) {
        appendError(err);
      }
      timer.stop();
      resultStats.push({
        name: testName + "," + modeName,
        time: timer.interval
      });
      setImmediate(operationsDoneCallback);
    };

    /** Call the operation numberOfIterations times within a transaction
     * and then call the operationsDoneCallback
     */
    var eachOperationsLoop = function(err) {
      if(DETAIL) JSCRUND.udebug.log_detail('jscrund.eachOperationsLoop iteration:', iteration, 'err:', err);
      // check result
      if (err) {
        appendError(err);
      }
      // call implementation operation
      if (iteration < numberOfIterations) {
        operation.apply(JSCRUND.implementation, [parameters[iteration], eachOperationsLoop]);
        iteration++;
      } else {
        if(DETAIL) JSCRUND.udebug.log_detail('jscrund.eachOperationLoop iteration:', iteration, 'complete.');
        JSCRUND.implementation.commit(eachCommitDoneCallback);
      }
    };

    /** Call eachOperationsLoop for all of the tests in testNames
     */
    var eachTestsLoop = function() {
      testName = testNames[testNumber];
      operation = JSCRUND.implementation[testName];
      operationsDoneCallback = eachTestsLoop;
      if (testNumber < testNames.length) {
        if(options.use_gc) global.gc();  // Full GC between tests
        testNumber++;
        if(DETAIL) JSCRUND.udebug.log_detail('jscrund.eachTestsLoop', testNumber, 'of', testNames.length, ':', testName);
        iteration = 0;
        timer.start(modeName, testName, numberOfIterations);
        JSCRUND.implementation.begin(function(err) {
          setImmediate(eachOperationsLoop, null);
        });
      } else {
        // done with all each tests
        // stop timer and report
        testsDoneCallback();
      }
    };

    /** Check the results of a bulk execute.
     */
    var bulkCheckBatchCallback = function(err) {
      if(DETAIL) JSCRUND.udebug.log_detail('jscrund.bulkCheckBatchCallback', err);
      // check result
      if (err) {
        appendError(err);
      }
      timer.stop();
      resultStats.push({
        name: testName + "," + modeName,
        time: timer.interval
      });
      setImmediate(bulkTestsLoop);
    };

    /** Check the results of a bulk operation. It will be executed
     * numberOfIterations times per test.
     */
    var bulkCheckOperationCallback = function(err) {
      if(DETAIL) JSCRUND.udebug.log_detail('jscrund.bulkCheckOperationCallback', err);
      if (err) {
        appendError(err);
      }
    };

    /** Construct one batch and execute it for all of the tests in testNames
     */
    var bulkTestsLoop = function() {
      testName = testNames[testNumber];
      operation = JSCRUND.implementation[testName];
      operationsDoneCallback = bulkTestsLoop;
      if (testNumber < testNames.length) {
        if(options.use_gc) global.gc();  // Full GC between tests
        testNumber++;
        if(DETAIL) JSCRUND.udebug.log_detail('jscrund.bulkTestsLoop', testNumber, 'of', testNames.length, ':', testName);
        timer.start(modeName, testName, numberOfIterations);
        JSCRUND.implementation.createBatch(function(err) {
          for (iteration = 0; iteration < numberOfIterations; ++iteration) {
            operation.apply(JSCRUND.implementation, [parameters[iteration], bulkCheckOperationCallback]);
          }
          JSCRUND.implementation.executeBatch(bulkCheckBatchCallback);
        });
      } else {
        // done with all bulk tests
        testsDoneCallback();
      }
    };

    /** Run all modes specified in --modes: default is indy, each, bulk.
     * 
     */
    var modeLoop = function() {
      modeName = modeNames[modeNumber];
      mode = modes[modeNumber];
      testNumber = 0;
      if (modeNumber < modes.length) {
        modeNumber++;
         console.log('\njscrund.modeLoop ', modeName, "[ size",numberOfIterations,"]");
        mode.apply(runTests);
      } else {
        if (JSCRUND.errors.length !== 0) {
          console.log(JSCRUND.errors);
        }
        report();
        batchSizeLoop();
      }
    };

    function report() {
      var opNames, opTimes, r, adapter;

      adapter = options.adapter + (options.spi ? "(spi)" : "");
      opNames = "rtime[ms]," + adapter + '\t';
      opTimes = "" + numberOfIterations + '\t';

      while (r = resultStats.shift()) {
        opNames += r.name + '\t';
        opTimes += r.time + '\t';
      }

      if(! nReport++) {
        logFile.write(opNames + '\n');
      }

      logFile.write(opTimes + '\n');
    }

    /** Iterate over batch sizes
    */
    function batchSizeLoop() {
      if(batchLoopNumber < numberOfBatchLoops) {
        numberOfIterations = options.iterations[batchLoopNumber];
        batchLoopNumber++;
        parameters = generateAllParameters(numberOfIterations);
        modeNumber = 0;
        setImmediate(modeLoop);
      } else {
        mainTestLoop();
      }
    }

    /** Run test with all modes.
    */
    var mainTestLoop = function() {
      if (nRun++ >= nRuns) {
        console.log('\ndone: ' + nRuns + ' runs.');
        console.log("\nappending results to file: " + logFile.name);
        logFile.close();
        if (options.stats) {
          JSCRUND.stats.peek();
        }
        JSCRUND.implementation.close(function(err) {
          if(options.delay_post > 0) {
            console.log("Delaying", options.delay_post, "seconds");
            setTimeout(process.exit, 1000 * options.delay_post);
          } else { 
            process.exit(err ? 1 : 0);
          }
        });
      } else {
        console.log('\nRun #' + nRun + ' of ' + nRuns);
        batchLoopNumber = 0;
        if(nRun === 2 && (options.delay_pre > 0)) { 
          console.log("Waiting", options.delay_pre, "seconds...");
          setTimeout(batchSizeLoop, 1000 * options.delay_pre);
          options.delay_pre = 0;
        } else {
          batchSizeLoop();
        }
      }
    };

    // runTests starts here
    var modeTable = {
        'indy': indyTestsLoop,
        'each': eachTestsLoop,
        'bulk': bulkTestsLoop
    };
    var modes = [];
    for (var m = 0; m < modeNames.length; ++m) {
      modes.push(modeTable[modeNames[m]]);
    }

    console.log('running tests with options:\n', options);
    JSCRUND.implementation.initialize(options, function(err) {
      // initialization complete
      if (err) {
        console.log('Error initializing JSCRUND.implementation:', err);
        process.exit(1);
      } else {
        testsDoneCallback = modeLoop;
        mainTestLoop();
      }
    });
  };

  // create database
  JSCRUND.metadataManager = require("jones-ndb").getDBMetadataManager(properties);

  JSCRUND.metadataManager.runSQL(path.join(__dirname, "./create.sql"), function(err) {
    if (err) {
      console.log('Error creating tables.', err);
      process.exit(1);
    }
    
    // if database create successful, run tests
    runTests(options);
  });

}

main();

