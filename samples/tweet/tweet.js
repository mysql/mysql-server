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

var http = require('http'),
    url  = require('url');

//////////////////////////////////////////
/*  User Domain Object Constructors.

    These can be called in two different ways: first, by user code, 
    where they might take function arguments.  But secondly, when the
    mapping layer reads a row of a mapped table from the database, 
    it calls the constructor *with no arguments*.
    
    The constructors must test whether the first argument === undefined. 

    If the constructor fails to do this, and overwrites a value that 
    has just been read, read operations will fail with error WCTOR.
*/

function Tweet(author, message) {
  if(author !== undefined) {
    this.id = null;
    this.date_created = new Date();
    this.author = author;
    this.message = message;
  }
}

function Author(user_name, full_name) {
  if(user_name !== undefined) {
    this.user_name = user_name;
    this.full_name = full_name;
  }
}

function HashtagEntry(tag, tweet) {
  if(tag !== undefined) {
    this.hashtag = tag;
    this.tweet_id = tweet.id;
    this.date_created = tweet.date_created;
    this.author = tweet.author;  
  }
}

function Follow(source, dest) {
  if(source !== undefined) {
    this.source = source;
    this.dest = dest;
  }
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
    this.httpResponse.write(JSON.serialize(this.operation.result));
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

/* onComplete(): when a single operation is complete.
   * error       : error from operation; will be stored in operation.latestError
   * resultData  : Operation result; for a find(), this will be the found item
   * operation   : Operation just completed
   * nextCallback: if supplied, will be called with operation as argument
*/
function onComplete(error, resultData, operation, nextCallback) {
  operation.latestError = error;
  if(resultData)   {   operation.result = resultData;      }
  if(error)        {   operation.responder.setError(error); }
  if(nextCallback) {   nextCallback(operation);            }
}

/* onFinal(): after a command-line operation, exit.
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
  operation.session.currentTransaction().rollback(function() {onFinal(operation)});
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

/* Returns an array of hashtags present in message 
*/
function extractHashtags(message) {
  var words, tags, word;
  tags = [];
  words = message.split(/\s+/);
  word = words.pop();
  while(word !== undefined) {
    if(word.charAt(0) == "#") {
      tags.push(/#(\w+)/.exec(word)[1]);
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
// Each instance has several properties defined in the Operation constructor,
// and a run() method.
//
// A multi-step operation possibly has other properties used to pass data
// from one step to the next
////////////////////////////////

function Operation() {
  this.run         = {};    // a run() method
  this.session     = {};    // This will be set before control reaches run()
  this.latestError = {};    // This must be set in every callback
  this.responder   = {};    // OperationResponder for this operation
  this.result      = {};    // Result object to be returned to the user
  this.isServer    = false; // True only for the start-HTTP-server operation
}


/* Add a user 
*/
function AddUserOperation(user_name, full_name) {
  var author = new Author(user_name, full_name);;
  Operation.call(this);    /* inherit */

  this.run = function() {
    this.session.persist(author, onComplete, author, this, onFinal);
  };
}


/* Profile a user based on username.
   This calls find(), then stores the result in self.object
*/
function LookupUserOperation(user_name) {
  Operation.call(this);    /* inherit */

  this.run = function() {
    this.session.find(Author, user_name, onComplete, this, onFinal);
  }
}


/* Insert a tweet.
     - Start a transaction.
     - Persist the tweet.  After persist, the tweet's auto-increment id 
       is available (and used by the HashtagEntry constructor).  
     - Create & persist Hashtag records.
     - Increment the author's tweet count.
     - Then commit the transaction. 
*/
function InsertTweetOperation(author, message) {
  Operation.call(this);    /* inherit */
  var tweet = new Tweet(author, message);
  var session;

  function doCommit(error, self) {
    if(error) {
      doRollback(self);
    }
    else {
      session.currentTransaction().commit(onComplete, null, self, onFinal);
    }
  }

  function onTweetCreateHashtagEntries(self) {
    var tags, tag, hashtagEntry, authorRecord;

    if(self.latestError) {
      doRollback(self);
    }
    else {
      tags = extractHashtags(message);
      tag = tags.pop();
      while(tag !== undefined) {
        hashtagEntry = new HashtagEntry(tag, tweet);
        session.persist(hashtagEntry, onComplete, "hashtag " + tag, self);
        tag = tags.pop();
      }
      authorRecord = new Author(author);  // Increment author's tweet count 
      increment(self, authorRecord, "tweets", doCommit);
    }
  }

  this.run = function() {   /* Start here */
    session = this.session;         
    session.currentTransaction().begin();
    session.persist(tweet, onComplete, tweet, this, onTweetCreateHashtagEntries);
  };
}


/* Delete a tweet.
   Relies on cascading delete to remove hashtag entries.
*/
function DeleteTweetOperation(tweet_id) { 
  Operation.call(this);    /* inherit */
  this.run = function() {
    this.session.remove(Tweet, tweet_id, onComplete, 
                        {"deleted": tweet_id},  this, onFinal);
  };
}


function TimelineOperation() {
  Operation.call(this);

  function buildQuery(error, query, self) {
    query.execute(onComplete, self, onFinal);
  }

  this.run = function() {
    this.session.createQuery(Tweet, buildQuery, this);
  }
}

/* The web server Operation is different:
   * it defines isServer = true.
   * it defines a runServer() method, which takes a SessionFactory.
*/
function RunWebServerOperation(port) {
  var sessionFactory;
  this.isServer = true;
  
  function serverRequestLoop(request, response) {
    var command = url.parse(request.url).pathname.split("/");
    var operation = parse_command(command);
  
  }


  this.runServer = function(_sessionFactory) {
    sessionFactory = _sessionFactory;
    http.createServer(serverRequestLoop).listen(port);
    console.log("Server started");  
  }


}


function parse_command(list) {
  switch(list[0]) {
    case 'newuser':
      return new AddUserOperation(list[1], list[2]);
    case 'whois':
      return new LookupUserOperation(list[1]);
    case 'insert':
      return new InsertTweetOperation(list[1], list[2]);
    case 'delete':
      return new DeleteTweetOperation(list[1]);
    case 'server':
      return new RunWebServerOperation(list[1]);
    case 'timeline':
      return new TimelineOperation();
    default:
      break;
  }
    return null;
} 


function get_cmdline_args() { 
  var i, val, operation;
  var cmdList = [];
  var usageMessage = 
    "Usage: node tweet {options} {command} {command arguments}\n" +
    "         -h or --help: print this message\n" +
    "         -d or --debug: set the debug flag\n" +
    "              --detail: set the detail debug flag\n" +
    "  XXX --adapter=<adapter>: run on the named adapter (e.g. ndb or mysql)\n" +
    "\n" +
    "  COMMANDS:\n" +
    "         newuser <user_name> <full_name>\n" +
    "         whois <user_name\n" +
    "         insert  <author> <message>\n" +
    "         delete  <tweet_id>\n" +
    "         server  <port>\n"
    ;

  for(i = 2; i < process.argv.length ; i++) {
    val = process.argv[i];
    switch (val) {
      case '--debug':
      case '-d':
        unified_debug.on();
        unified_debug.level_debug();
        break;
      case '--detail':
        unified_debug.on();
        unified_debug.level_detail();
        break;
      case '--help':
      case '-h':
        break;
      default:
        cmdList.push(val);
    }
  }

  if(cmdList.length) {
    operation = parse_command(cmdList);
  }

  if(operation) {
    return operation;
  }
  else {    
    console.log(usageMessage);
    process.exit(1);
  }
}




/////////// MAIN:


// *** program starts here ***

/* Global Variable Declarations */
var nosql = require('../..');
var adapter;
var mappings;
var dbProperties;
var operation;

/* Default Values */
adapter = "ndb";

function runCmdlineOperation(err, sessionFactory, operation) {
  if(err) {
    console.log(err);
    process.exit(1);
  }
  if(operation.isServer) {
    operation.runServer(sessionFactory);
  }
  else {
    operation.responder = new ConsoleOperationResponder(operation);
    sessionFactory.openSession(null, onOpenSession, operation);
  }
}


// Map SQL Tables to JS Constructors using default mappings
mappings = [];
mappings.push(new nosql.TableMapping('tweet').applyToClass(Tweet));
mappings.push(new nosql.TableMapping('author').applyToClass(Author));
mappings.push(new nosql.TableMapping('hashtag').applyToClass(HashtagEntry));
mappings.push(new nosql.TableMapping('follow').applyToClass(Follow));

mappings = [ Tweet, Author, HashtagEntry, Follow ];

// udebug.log('Running with adapter', adapter);
//create a database properties object

dbProperties = nosql.ConnectionProperties(adapter);
operation = get_cmdline_args();
nosql.connect(dbProperties, mappings, runCmdlineOperation, operation);

