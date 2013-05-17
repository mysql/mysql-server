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

/*global JSCRUND */
global.JSCRUND = {};

JSCRUND.mynode = require("..");
JSCRUND.random = require("../samples/ndb_loader/lib/RandomData");
JSCRUND.unified_debug = require("../Adapter/api/unified_debug");
JSCRUND.udebug = JSCRUND.unified_debug.getLogger('jscrund');
JSCRUND.stats  = require("../Adapter/api/stats");
JSCRUND.lib    = require("./lib");
JSCRUND.mysqljs = require("./jscrund_mysqljs");
JSCRUND.errors  = [];


function usage() {
  var msg = "" +
  "Usage:  node jscrund [options]\n" +
  "   --help:\n" +
  "   -h      :  Print help (this message)\n" +
  "   --adapter=ndb\n" +
  "   -n      :  Use ndb adapter (default)\n" +
  "   --adapter=mysql\n" +
  "   -m      :  Use mysql adapter\n" +
  "   --adapter=sql\n" +
  "   -f      :  Use felix sql driver (not mysql-js api)\n" +
  "   --detail:  Enable detail debug output\n" +
  "   --debug :\n" +
  "   -d      :  Enable debug output\n" +
  "   -df=file:  Enable debug output only from source file <file>\n" +
  "   --debugFile=<file>  \n" +
  "   -i      :  Specify number of iterations per test (default 4000)\n" +
  "   --modes :\n" +
  "   --mode  :  Specify modes to run (default indy,each,bulk)\n" +
  "   --tests :\n" +
  "   --test  :  Specify tests to run (default persist,find,remove)\n" +
  "   --table':\n" +
  "   -t      :  Use table name for operations\n" +
  "  --forever:  Repeat tests until interrupted\n" +
  "   --set other=value: set property other to value"
  ;
  console.log(msg);
  process.exit(1);
}

//handle command line arguments
function parse_command_line(options) {
  var i, val, values, pair;
  for(i = 2; i < process.argv.length ; i++) {
    val = process.argv[i];
    switch (val) {
    case '--help':
    case '-h':
      options.exit = true;
      break;
    case '-n':
      options.adapter = "ndb";
      break;
    case '-m':
      options.adapter = "mysql";
      break;
    case '-i':
      options.iterations = parseInt(process.argv[++i]);
      if (isNaN(options.iterations)) {
        console.log('iterations value is not allowed:', process.argv[i]);
        options.exit = true;
      }
      break;
    case '--stats':
      options.stats = true;
      break;
    case '--forever':
      options.forever = true;
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
    case '--set':
      i++;  // next argument
      pair = process.argv[i].split('=');
      if(pair.length === 2) {
        JSCRUND.udebug.log("Setting global:", pair[0], "=", pair[1]);
        global[pair[0]] = pair[1];
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
          for (var m = 0; m < options.modeNames.length; ++m) {
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
              break;
            default:
              console.log('Invalid test ' + options.testNames[t]);
              options.exit = true;
            }
          }
          break;
        case '--debugFile':
        case '-df':
          unified_debug.on();
          var client = require(path.join(build_dir,"ndb_adapter")).debug;
          unified_debug.register_client(client);
          unified_debug.set_file_level(values[1], 5);
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

/** Constructor for domain object for A mapped to table a.
 */
function A() {
}

/** Options are set up based on command line.
 * Properties are set up based on options.
 */
function main() {
  var config_file_exists = false;

  /* Default options: */
  var options = {
    'adapter' : 'ndb',
    'database': 'jscrund',
    'modes': 'indy,each,bulk',
    'tests': 'persist,find,remove',
    'iterations': 4000,
    'stats': false,
    'forever': false
  };

  /* Options from config file; connection_properties are handled below */
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
    console.log(e);
    if (e.message.indexOf('Cannot find module') === -1) {
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

  var properties = {};
  if (options.adapter === 'ndb' || options.adapter === 'mysql') {
    properties = new JSCRUND.mynode.ConnectionProperties(options.adapter);
    JSCRUND.implementation = new JSCRUND.mysqljs.implementation();
  } else if (options.adapter === 'sql') {
    var sqladapter = require('./jscrund_sql');
    JSCRUND.implementation = new sqladapter.implementation();
  }
  /* Connection properties from jscrund.config */
  if(config_file_exists) {
    for(var i in config_file.connection_properties) {
      if(config_file.connection_properties.hasOwnProperty(i)) {
        properties[i]  = config_file.connection_properties[i];
      }
    }
  }
  
  properties.database = options.database;
  options.properties = properties; // properties for getSession
  new JSCRUND.mynode.TableMapping("a").applyToClass(A);
  options.annotations = A;

  var generateAllParameters = function(numberOfParameters) {
    var result = [];
    for (var i = 0; i < numberOfParameters; ++i) {
      result[i] = generateParameters(i);
    }
    return result;
  };

  var generateParameters = function(i) {
    return {'key' :generateKey(i), 'object':generateObject(i)};
  };

  var generateKey = function(i) {
    return i;
  };

  var generateObject = function(i) {
    var result = new A();
    result.id = i;
    result.cint = i;
    result.clong = i;
    result.cfloat = i;
    result.cdouble = i;
    return result;
  };

  var appendError = function(error) {
    JSCRUND.errors.push(error);
    if ((options.printStackTraces) && typeof(error.stack) !== 'undefined') {
      JSCRUND.errors.push(error.stack);
    }
  };

  var runTests = function(options) {

    var timer = new JSCRUND.lib.Timer();

    var modeNames = options.modes.split('\,');    
    var modeNumber = 0;
    var mode;
    var modeName;

    var testNames = options.tests.split('\,');
    var testNumber = 0;
    var operation;
    var testName;

    var key;
    var object;
    var iteration = 0;
    var numberOfIterations = options.iterations;
    var iterationsCompleted = 0;

    var operationsDoneCallback;
    var testsDoneCallback;
    var resultsArray = [];

    var parameters = generateAllParameters(numberOfIterations);
    
    /** Recursively call the operation numberOfIterations times in autocommit mode
     * and then call the operationsDoneCallback
     */
    var indyOperationsLoop = function(err) {
      JSCRUND.udebug.log_detail('jscrund.indyOperationsLoop iteration:', iteration, 'err:', err);
      // check result
      if (err) {
        appendError(err);
      }
      // call implementation operation
      if (iteration < numberOfIterations) {
        operation.apply(JSCRUND.implementation, [parameters[iteration], indyOperationsLoop]);
        iteration++;
      } else {
        JSCRUND.udebug.log_detail('jscrund.indyOperationsLoop iteration:', iteration, 'complete.');
        timer.stop();
        resultsArray.push(timer.interval);
        operationsDoneCallback();
      }
    };


    /** Call indyOperationsLoop for all of the tests in testNames
     */
    var indyTestsLoop = function() {
      testName = testNames[testNumber];
      operation = JSCRUND.implementation[testName];
      operationsDoneCallback = indyTestsLoop;
      if (testNumber < testNames.length) {
        testNumber++;
        JSCRUND.udebug.log_detail('jscrund.indyTestsLoop', testNumber, 'of', testNames.length, ':', testName);
        iteration = 0;
        timer.start(modeName, testName, numberOfIterations);
        indyOperationsLoop(null);
      } else {
        // done with all indy tests
        // stop timer and report
        testsDoneCallback();
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
      resultsArray.push(timer.interval);
      operationsDoneCallback();
    };

    /** Call the operation numberOfIterations times within a transaction
     * and then call the operationsDoneCallback
     */
    var eachOperationsLoop = function(err) {
      JSCRUND.udebug.log_detail('jscrund.eachOperationsLoop iteration:', iteration, 'err:', err);
      // check result
      if (err) {
        appendError(err);
      }
      // call implementation operation
      if (iteration < numberOfIterations) {
        operation.apply(JSCRUND.implementation, [parameters[iteration], eachOperationsLoop]);
        iteration++;
      } else {
        JSCRUND.udebug.log_detail('jscrund.eachOperationLoop iteration:', iteration, 'complete.');
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
        testNumber++;
        JSCRUND.udebug.log_detail('jscrund.eachTestsLoop', testNumber, 'of', testNames.length, ':', testName);
        iteration = 0;
        timer.start(modeName, testName, numberOfIterations);
        JSCRUND.implementation.begin(function(err) {
          eachOperationsLoop(null);
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
      JSCRUND.udebug.log_detail('jscrund.bulkCheckBatchCallback', err);
      timer.stop();
      resultsArray.push(timer.interval);
      if (err) {
        appendError(err);
      }
      bulkTestsLoop();
    };

    /** Check the results of a bulk operation. It will be executed
     * numberOfIterations times per test.
     */
    var bulkCheckOperationCallback = function(err) {
      JSCRUND.udebug.log_detail('jscrund.bulkCheckOperationCallback', err);
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
        testNumber++;
        JSCRUND.udebug.log_detail('jscrund.bulkTestsLoop', testNumber, 'of', testNames.length, ':', testName);
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

    /** Finish up after modeLoop
    */
    var onLoopComplete = function() {
      if(options.stats) {
        JSCRUND.stats.peek();
      }
      if(options.forever) {
        runTests(options);
      }
      else {
        process.exit(0);
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
        console.log('\njscrund.modeLoop', modeNumber, 'of', modes.length, ':', modeName);
        mode.apply(runTests);
      } else {
        console.log('jscrund.modeLoop', modeNumber, 'of', modes.length, 'complete.');
        if (JSCRUND.errors.length !== 0) {
          console.log(JSCRUND.errors);
        }
        var r, resultsString = "";
        while(r = resultsArray.shift())
          resultsString += r + "\t";
        console.log(resultsString);
        JSCRUND.implementation.close(onLoopComplete);
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
        console.log('Error initializing', err);
        process.exit(1);
      } else {
        testsDoneCallback = modeLoop;
        modeLoop();
      }
    });
  };

  // create database
  JSCRUND.lib.SQL.create('./', properties, function(err) {
    if (err) {
      console.log('Error creating tables.', err);
      process.exit(1);
    }
    
    // if database create successful, run tests
    runTests(options);
  });

}

main();

