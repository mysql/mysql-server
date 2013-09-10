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


/* Simple Twitter-like application for mysql-js
 *
 * This depends on the schema defined in tweet.sql
 *
 * It connects to the database using the properties defined in
 * tweet.properties.js
 *
 *
 *
 *
 *
 *
 */

'use strict';

var http   = require('http'),
    url    = require('url'),
    nosql  = require('../..'),
    udebug = unified_debug.getLogger("tweet.js"),
    getProperties = require("./tweet.properties.js").getProperties,
    adapter = "ndb",
    mainLoopComplete, parse_command;

//////////////////////////////////////////
/*  User Domain Object Constructors.

    These can be called in two different ways: first, by user code, 
    where they might take function arguments.  But secondly, when the
    mapping layer reads a row of a mapped table from the database, 
    it calls the constructor *with no arguments*.
    
    So the constructors must test whether the first argument === undefined. 
    If the constructor fails to do this, and overwrites the value just read
    from the database, the read operation will fail with error WCTOR.
*/

function Author(user_name, full_name) {
  if(user_name !== undefined) {
    this.user_name = user_name;
    this.full_name = full_name;
  }
}

function Tweet(author, message) {
  if(author !== undefined) {
    this.id = null;
    this.date_created = new Date();
    this.author = author;
    this.message = message;
  }
}

function HashtagEntry(tag, tweet) {
  if(tag !== undefined) {
    this.hashtag = tag;
    this.tweet_id = tweet.id;
  }
}

function AtRef(at_user, tweet) {
  if(at_user !== undefined) {
    this.at_user = at_user;
    this.tweet_id = tweet.id;
  }
}

function Follow(follower, followed) {
  if(follower !== undefined) {
    this.follower = follower;
    this.followed = followed;
  }
  this.toString = function() {
    return this.follower + " follows " + this.followed;
  };
}

//////////////////////////////////////////

// OperationResponder is a common interface over 
// two sorts of responses (console and HTTP)

function ConsoleOperationResponder(operation) {
  this.operation  = operation;
  this.error      = false;
  this.setError   = function(e) { this.error = e; };
  this.close      = function()  { 
    if(this.error) { console.log("Error", this.error); }
    else           { console.log("Success", this.operation.result); }
    this.operation.session.close(mainLoopComplete);
  };
}

function HttpOperationResponder(operation, httpResponse) {
  this.operation    = operation;
  this.httpResponse = httpResponse;
  this.statusCode   = 200;
  this.setError     = function(e) { 
    this.statusCode = 500;
    this.setResult(e);
  };
  this.close        = function() {
    this.httpResponse.statusCode = this.statusCode;
    this.httpResponse.write(JSON.stringify(this.operation.result));
    this.httpResponse.write("\n");
    this.httpResponse.end();
    this.operation.session.close();
  }; 
}

//////////////////////////////////////////


// ======== Callbacks run at various phases of a database operation ======= //

/* mainLoopComplete() callback.  This runs after the single operation 
   given on the command line, or after the web server has shut down.
*/
function mainLoopComplete() {
  console.log("--FINISHED--");
  process.exit(0);
}

/* onComplete(): when a single database operation is complete.
   * error       : error from operation; will be stored in operation.latestError
   * resultData  : Operation result; for a find(), this will be the found item
   * operation   : Operation just completed
   * nextCallback: if supplied, will be called with operation as argument
*/
function onComplete(error, resultData, operation, nextCallback) {
  udebug.log("onComplete :", error, ":", resultData, ":", operation.name);
  operation.latestError = error;
  if(resultData)   {   operation.result = resultData;       }
  if(error)        {   operation.responder.setError(error); }
  if(nextCallback) {   nextCallback(operation);             }
}

/* onFinal(): When an Operation is complete, close its responder. 
*/
function onFinal(operation) {
  operation.responder.close();
}

/* onOpenSession(): bridge from openSession() to operation.run()
*/
function onOpenSession(error, session, operation) {
  operation.latestError = error;
  operation.session = session;
  if(session && ! error) {
    operation.run();
  }
  else {
    onComplete(error, null, operation, onFinal);
  }
}

/* doRollback(): roll back a failed transaction
*/
function doRollback(operation) {
  operation.session.currentTransaction().rollback(function() {onFinal(operation);});
}

/* Increment a stored counter 
*/
function increment(operation, object, property, callback) {
  function onRead(error) {
    operation.latestError = error;
    if(error) {
      callback(error, operation);
    }
    else {
      object[property] += 1;
      operation.session.update(object, callback, operation);
    }
  }

  operation.session.load(object, onRead);
}

////////////////////////////////
//  Application Logic
////////////////////////////////

/* Returns arrays of #hashtags and @atrefs present in message 
*/
function extractTags(message) {
  var words, word, tags, tag;
  tags = {"hash" : [], "at" : [] };
  words = message.split(/\s+/);
  word = words.pop();
  while(word !== undefined) {
    if(word.charAt(0) == "#") {
      tag = /#(\w+)/.exec(word)[1];
      if(tags.hash.indexOf(tag) == -1) {
        tags.hash.push(tag);
      }
    }
    else if(word.charAt(0) == "@") {
      tag = /@(\w+)/.exec(word)[1];
      if(tags.at.indexOf(tag) == -1) {
        tags.at.push(tag);
      }
    }
    word = words.pop();
  }
  return tags;
}


////////////////////////////////
// BASIC OPERATIONS
//
// All actions (insert, delete, start the http server) are represented
// by instances of Operation.
// 
// 
// Each instance has several properties defined in the Operation constructor,
// and a run() method.
//
// A multi-step operation possibly has other properties used to pass data
// from one step to the next
////////////////////////////////

function Operation() {
  this.run          = {};    // a run() method
  this.session      = {};    // This will be set before control reaches run()
  this.latestError  = {};    // This must be set in every callback
  this.responder    = {};    // OperationResponder for this operation
  this.result       = {};    // Result object to be returned to the user
  this.data         = {};    // HTTP POST data
  this.isServer     = false; // True only for the start-HTTP-server operation
  this.setResponder = function(x) { this.responder = x; };
}


// Each Operation constructor takes (params, data)
// Params is an array of command-line or URL parameters, starting at 1
// (as params[0] specified the operation itself).
// Data is HTTP POST data


/* Add a user 
*/
function AddUserOperation(params, data) {
  var author = new Author(params[1], data);
  Operation.call(this);    /* inherit */

  this.run = function() {
    this.session.persist(author, onComplete, author, this, onFinal);
  };
}


/* Profile a user based on username.
   This calls find(), then stores the result in self.object
*/
function LookupUserOperation(params, data) {
  Operation.call(this);    /* inherit */

  this.run = function() {
    this.session.find(Author, params[1], onComplete, this, onFinal);
  };
}


/* Insert a tweet.
     - Start a transaction.
     - Persist the tweet.  After persist, the tweet's auto-increment id 
       is available (and used by the HashtagEntry constructor).  
     - Create & persist Hashtag records.
     - Increment the author's tweet count.
     - Then commit the transaction. 
*/
function InsertTweetOperation(params, data) {
  Operation.call(this);    /* inherit */
  var session;
  var author = params[1];
  var message = data;
  var tweet = new Tweet(author, message);

  this.name = "insertTweet";
  function doCommit(error, self) {
    if(error) {
      doRollback(self);
    }
    else {
      session.currentTransaction().commit(onComplete, null, self, onFinal);
    }
  }

  function incrementTweetCount(self) {
    var authorRecord = new Author(author);  // Increment author's tweet count 
    increment(self, authorRecord, "tweets", doCommit);
  }

  function onTweetCreateTagEntries(self) {
    var batch, tags, tag, tagEntry, authorRecord;

    if(self.latestError) {
      doRollback(self);
    }
    else {
      /* Store all #hashtag and @atref entries in a single batch */

      tags = extractTags(message);
      batch = session.createBatch();

      tag = tags.hash.pop();   // # hashtags
      while(tag !== undefined) {
        tagEntry = new HashtagEntry(tag, tweet);
        batch.persist(tagEntry, onComplete, null, self);
        tag = tags.hash.pop();
      }
      
      tag = tags.at.pop();   // @ mentions
      while(tag != undefined) {
        tagEntry = new AtRef(tag, tweet);
        batch.persist(tagEntry, onComplete, null, self);
        tag = tags.at.pop();
      }

      batch.execute(onComplete, null, self, incrementTweetCount);
    }
  }

  this.run = function() {   /* Start here */
    session = this.session;         
    session.currentTransaction().begin();
    session.persist(tweet, onComplete, tweet, this, onTweetCreateTagEntries);
  };
}


/* Delete a tweet.
   Relies on cascading delete to remove hashtag entries.
*/
function DeleteTweetOperation(params, data) { 
  Operation.call(this);    /* inherit */
  var tweet_id = params[1];
  this.run = function() {
    this.session.remove(Tweet, tweet_id, onComplete, 
                        {"deleted": tweet_id},  this, onFinal);
  };
}


/* Get a tweet by id.
*/
function ReadTweetOperation(params, data) {
  Operation.call(this);
  this.run = function() {
    this.session.find(Tweet, params[1], onComplete, this, onFinal);
  };
}

/* Make user A a follower of user B
*/
function FollowOperation(params, data) {
  Operation.call(this);
  this.run = function() {
    var record = new Follow(params[1], params[2]);
    this.session.persist(Follow, record, onComplete, record.toString(), 
                         this, onFinal);
  };
}

/* Who follows User A?
*/
function FollowersOperation(params, data) {
  Operation.call(this);
  
  function buildQuery(error, query, self) {
    query.where(query.followed.eq(query.param("who")));
    query.execute({who:params[1]}, onComplete, self, onFinal);  
  }

  this.run = function() {
    this.session.createQuery(Follow, buildQuery, this);    
  };
}


function FollowingOperation() {}

function TimelineOperation(params, data) {
  Operation.call(this);

  function buildQuery(error, query, self) {
    query.where(query.author.eq(query.param("who")));
    query.execute({who:"mr_dog"}, onComplete, self, onFinal);
  }

  this.run = function() {
    this.session.createQuery(Tweet, buildQuery, this);
  };
}

/* The web server Operation is different:
   * it defines isServer = true.
   * it defines a runServer() method, which takes a SessionFactory.
*/
function RunWebServerOperation(cli_params, cli_data) {
  var port = cli_params[1];
  var sessionFactory;
  this.isServer = true;
  
  function serverRequestLoop(request, response) {
    var params, data;

    function hangup(code) { 
      response.statusCode = code;
      response.end();
    }

    function runOperation() {
      var operation = parse_command(params, data);
      if(operation && ! operation.isServer) {
        operation.setResponder(new HttpOperationResponder(operation, response));
        sessionFactory.openSession(null, onOpenSession, operation);  
      } 
      else hangup(400);
    }
    
    function gatherData(chunk) {
      data += chunk;
    }

    request.setEncoding('utf8');
    request.on('data', gatherData);  
    request.on('end', runOperation);

    data = "";
    params = url.parse(request.url).pathname.split("/");
    params.shift();
  
    switch(request.method) {
      case 'GET':
        runOperation();
        break;
      case 'POST':
        /* runOperation() is called by the 'end' event handler */
        break;
      default:
        hangup(405);
    }    
  }

  this.runServer = function(_sessionFactory) {
    sessionFactory = _sessionFactory;
    http.createServer(serverRequestLoop).listen(port);
    console.log("Server started on port", port);  
  };
}


function parse_command(params, data) {
  var keyWordToOperationMap, opConstructor, operation;
  operation = null;

  keyWordToOperationMap = {
    'newuser'     :    AddUserOperation          ,
    'whois'       :    LookupUserOperation       ,
    'insert'      :    InsertTweetOperation      ,
    'delete'      :    DeleteTweetOperation      ,
    'read'        :    ReadTweetOperation        ,
    'follow'      :    FollowOperation           ,
    'followers'   :    FollowersOperation        ,
    'following'   :    FollowingOperation        ,
    'server'      :    RunWebServerOperation     , 
    'timeline'    :    TimelineOperation
  };

  opConstructor = keyWordToOperationMap[params[0]];
  if(opConstructor) {
    operation = {};
    opConstructor.call(operation, params, data);
  }
  
  return operation;
}


function get_cmdline_args() { 
  var i, val, operation;
  var cmdList = [];
  var usageMessage = 
    "Usage: node tweet {options} {command} {command arguments}\n" +
    "         -a <adapter>:  run using the named adapter (default: ndb)\n" + 
    "         -h or --help: print this message\n" +
    "         -d or --debug: set the debug flag\n" +
    "               --detail: set the detail debug flag\n" +
    "               -df <file>: enable debug output from <file>\n" +
    "\n" +
    "  COMMANDS:\n" +
    "         newuser <user_name> <full_name>\n" +
    "         whois <user_name\n" +
    "         insert  <author> <message>\n" +
    "         read <tweet_id>\n" + 
    "         delete  <tweet_id>\n" +
    "         server  <port>\n" +
    "         followers <user_name>\n"
    ;

  for(i = 2; i < process.argv.length ; i++) {
    val = process.argv[i];
    switch (val) {
      case '-a':
        adapter = process.argv[++i];
        break;
      case '--debug':
      case '-d':
        unified_debug.on();
        unified_debug.level_debug();
        break;
      case '--detail':
        unified_debug.on();
        unified_debug.level_detail();
        break;
      case '-df':
        unified_debug.set_file_level(process.argv[++i], 5);
        break;
      case '--help':
      case '-h':
        break;
      default:
        cmdList.push(val);
    }
  }

  if(cmdList.length) {
    /* Use final command line argument as "data" */
    operation = parse_command(cmdList, cmdList[cmdList.length-1]);
  }

  if(operation) {
    return operation;
  }
   
  console.log(usageMessage);
  process.exit(1);
}


/* Run a single operation specified on the command line
*/
function runCmdlineOperation(err, sessionFactory, operation) {
  if(err) {
    console.log(err);
    process.exit(1);
  }

  if(operation.isServer) {
    operation.runServer(sessionFactory);
  }
  else {
    operation.setResponder(new ConsoleOperationResponder(operation));
    sessionFactory.openSession(null, onOpenSession, operation);
  }
}

// *** Main program starts here ***

/* Global Variable Declarations */
var mappings, dbProperties, operation;

/* This may have the side effect of changing the adapter */
operation = get_cmdline_args();

/* Connection Properties */
dbProperties = getProperties(adapter);

// Map SQL Tables to JS Constructors using default mappings
mappings = [];
mappings.push(new nosql.TableMapping('tweet').applyToClass(Tweet));
mappings.push(new nosql.TableMapping('author').applyToClass(Author));
mappings.push(new nosql.TableMapping('hashtag').applyToClass(HashtagEntry));
mappings.push(new nosql.TableMapping('follow').applyToClass(Follow));
mappings.push(new nosql.TableMapping('atref').applyToClass(AtRef));

nosql.connect(dbProperties, mappings, runCmdlineOperation, operation);

