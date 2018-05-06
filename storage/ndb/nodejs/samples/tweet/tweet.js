/* Simple Twitter-like application for mysql-js
 *
 * This depends on the schema defined in create_tweet_tables.sql
 *
 */

'use strict';

var http   = require('http'),
    assert = require('assert'),
    url    = require('url'),
    jones  = require('database-jones'),
    udebug = unified_debug.getLogger("tweet.js"),
    adapter = process.env.JONES_ADAPTER || "ndb",
    deployment = process.env.JONES_DEPLOYMENT || "test",
    mainLoopComplete, parse_command, allOperations, operationMap;

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
function Author(user_name, propertiesString) {
  var properties, p;
  if(user_name !== undefined) {
    this.user_name = user_name;
    this.tweet_count = 0;
    if(typeof propertiesString === 'string') {
      try {
        properties = JSON.parse(propertiesString);
        for(p in properties) {
          if(properties.hasOwnProperty(p)) { this[p] = properties[p]; }
        }
      }
      catch(e) { console.log(e); }
    }
  }
}

function Tweet(author, message) {
  if(author !== undefined) {
    this.date_created = new Date();
    this.author_user_name = author;
    this.message = message;
  }
}

function HashtagEntry(tag, tweet) {
  if(tag !== undefined) {
    this.hashtag = tag;
    this.tweet_id = tweet.id;
  }
}

function Mention(at_user, tweet) {
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
}

Follow.prototype.toString = function() {
  return this.follower + " follows " + this.followed;
};

// OperationResponder is a common interface over 
// two sorts of responses (console and HTTP)

function ConsoleOperationResponder(operation) {
  udebug.log("new ConsoleOperationResponder");
  this.operation  = operation;
  this.error      = false;
}

ConsoleOperationResponder.prototype.setError = function(e) {
 this.error = e;
 udebug.log(e.stack);
};

ConsoleOperationResponder.prototype.write = function(x) {
  console.log(x);
};

ConsoleOperationResponder.prototype.close = function() {
    if(this.error) { console.log("Error", this.error); }
    else           { console.log("Success", this.operation.result); }
    this.operation.session.close(mainLoopComplete);
};


function HttpOperationResponder(operation, httpResponse) {
  udebug.log("new HttpOperationResponder");
  this.operation    = operation;
  this.httpResponse = httpResponse;
  this.statusCode   = 200;
}

HttpOperationResponder.prototype.setError = function(e) { 
  this.statusCode = 500;
  this.operation.result = e;
};

HttpOperationResponder.prototype.write = function(x) {
  this.httpResponse.write(x);
};

HttpOperationResponder.prototype.close = function() {
  this.httpResponse.statusCode = this.statusCode;
  this.write(JSON.stringify(this.operation.result));
  this.write("\n");
  this.httpResponse.end();
  this.operation.session.close();
}; 

// ======== Callbacks run at various phases of a database operation ======= //

/* mainLoopComplete() callback.  This runs after the single operation 
   given on the command line, or after the web server has shut down.
*/
function mainLoopComplete() {
  console.log("--FINISHED--");
  process.exit(0);
}

/* onOpenSession(): bridge from openSession() to operation.run()
*/
function onOpenSession(session, operation) {
  udebug.log("onOpenSession");
  assert(session);
  operation.session = session;
  operation.run(session);
}


/* extractTags() returns arrays of #hashtags and @mentions present in a message 
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

/* Takes query and params; builds an equal condition on each param.
*/
function buildQueryEqual(query, params) {
  var field;
  for(field in params) {
    if(params.hasOwnProperty(field) && query[field]) { 
      query.where(query[field].eq(query.param(field)));
    }
  }
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
// A multi-step operation might have additional properties used to pass data
// from one step to the next
////////////////////////////////

function Operation() {
  this.run          = {};    // a run() method
  this.session      = {};    // This will be set in onOpenSession
  this.responder    = {};    // OperationResponder for this operation
  this.result       = {};    // Result object to be returned to the user
  this.data         = {};    // HTTP POST data
  this.isServer     = false; // True only for the start-HTTP-server operation
  this.queryClass   = null;  // Mapped class to use in creating a query
  this.queryParams  = null;  // Parameters to use for query

  /* Functions closed over self */
  var self = this;
  
  this.setResult = function(value) {
    udebug.log("Operation setResult", value);
    self.result = value;
  };
  
  this.onComplete = function() {
    udebug.log("Operation onComplete");
    self.responder.close();
  };

  this.onError = function(error) {
    udebug.log("Operation onError");
    self.responder.setError(error);
    self.responder.close();  
  };

  this.buildQuery = buildQueryEqual; 

  this.onQueryResults = function(error, results) { 
    if(error) { self.onError(error); } 
    else      { self.setResult(results); self.onComplete(); }
  };

  this.buildQueryAndPresentResults = function(session) {
    function onCreateQuery(query) {
      self.buildQuery(query, self.queryParams);
      query.execute(self.queryParams, self.onQueryResults);
    }
    session.createQuery(self.queryClass).then(onCreateQuery, self.onError);
  };
}

// Each Operation constructor takes (params, data), where "params" is an array
// of URL or command-line parameters.  "data" is the HTTP POST data.
// In the command line version, the POST data comes from the final parameter
//
// An Operation Constructor also has a signature property, which is an array
// of [ verb, noun, parameters, data ] used in the API.  "verb" is an HTTP
// verb; noun and parameters are separated by slashes in the URL.


/* Add a user 
*/
function AddUserOperation(params, data) {
  var author = new Author(params[0], data);
  Operation.call(this);    /* inherit */

  this.run = function(session) {
    session.persist(author).
      then(function() { return author; }).
      then(this.setResult).
      then(this.onComplete, this.onError);
  };
}
AddUserOperation.signature = [ "put", "user", "<user_name>", " << JSON Extra Fields >>" ];


/* Profile a user based on username.
   This calls find(), then stores the result in self.object
*/
function LookupUserOperation(params, data) {
  Operation.call(this);    /* inherit */
  
  this.run = function(session) {
    session.find(Author, params[0]).
      then(this.setResult).
      then(this.onComplete, this.onError);      
  };
}
LookupUserOperation.signature = [ "get", "user", "<user_name>" ]; 


/* Delete a user, with cascading delete of tweets, mentions, and follows
*/
function DeleteUserOperation(params, data) { 
  Operation.call(this);    /* inherit */
  var author_name = params[0];
  
  this.run = function(session) {
    session.remove(Author, author_name).
      then(function() {return {"deleted": author_name};}).
      then(this.setResult).
      then(this.onComplete, this.onError);
  };
}
DeleteUserOperation.signature = [ "delete", "user", "<user_name>" ];


/* Insert a tweet.
     - Start a transaction.
     - Persist the tweet.  After persist, the tweet's auto-increment id 
       is available (and will be used by the HashtagEntry and Mention constructors).
     - Create & persist #hashtag & @user records (all in a single batch).
     - Increment the author's tweet count.
     - Then commit the transaction. 
*/
function InsertTweetOperation(params, data) {
  Operation.call(this);    /* inherit */

  var message = data;
  var authorName = params[0];
  var tweet = new Tweet(authorName, message);

  this.run = function(session) {   /* Start here */

    function createTagEntries() {
      udebug.log("onTweetCreateTagEntries");
      var batch, tags, tag, tagEntry;

      /* Store all #hashtag and @mention entries in a single batch */

      tags = extractTags(message);
      batch = session.createBatch();

      tag = tags.hash.pop();   // # hashtags
      while(tag !== undefined) {
        tagEntry = new HashtagEntry(tag, tweet);
        batch.persist(tagEntry);
        tag = tags.hash.pop(); 
      }
      
      tag = tags.at.pop();   // @ mentions
      while(tag !== undefined) {
        tagEntry = new Mention(tag, tweet);
        batch.persist(tagEntry);
        tag = tags.at.pop();
      }

      return batch.execute();    
    }

    function incrementTweetCount(author) {
      author.tweet_count++;
      return session.save(author);
    }
  
    function commitOnSuccess(value) {
      return session.currentTransaction().commit(); 
    }

    function rollbackOnError(err) {
      return session.currentTransaction().rollback();
    }

    session.currentTransaction().begin();
    session.persist(tweet).
      then(function() { return session.find(Author, authorName);}).
      then(incrementTweetCount).
      then(createTagEntries).
      then(commitOnSuccess, rollbackOnError).
      then(function() {return tweet;}).
      then(this.setResult).
      then(this.onComplete, this.onError);
  };
}
InsertTweetOperation.signature = [ "post", "tweet", "<author>", " << Message >>" ];


/* Delete a tweet.
   Relies on cascading delete to remove hashtag entries.
*/
function DeleteTweetOperation(params, data) { 
  Operation.call(this);    /* inherit */
  var tweet_id = params[0];

  this.run = function(session) {
    session.remove(Tweet, tweet_id).
      then(function() { return { "deleted" : tweet_id };}).
      then(this.setResult).
      then(this.onComplete, this.onError);
  };
}
DeleteTweetOperation.signature = [ "delete", "tweet", "<tweet_id>" ];


/* Get a tweet by id.
*/
function ReadTweetOperation(params, data) {
  Operation.call(this);

  this.run = function(session) {
    session.find(Tweet, params[0]).
      then(this.setResult).
      then(this.onComplete, this.onError);
  };
}
ReadTweetOperation.signature = [ "get", "tweet", "<tweet_id>" ];


/* Make user A a follower of user B
*/
function FollowOperation(params, data) {
  Operation.call(this);
  
  this.run = function(session) {
    var record = new Follow(params[0], params[1]);
    session.persist(record).
      then(function() { return record; }).
      then(this.setResult).
      then(this.onComplete, this.onError);
  };
}
FollowOperation.signature = [ "put", "follow", "<user_follower> <user_followed>"];


/* Who follows a user?
*/
function FollowersOperation(params, data) {
  Operation.call(this);  
  this.queryClass = Follow;
  this.queryParams =  {"followed" : params[0]};
  this.run = this.buildQueryAndPresentResults;
}
FollowersOperation.signature = [ "get", "followers", "<user_name>" ];


/* Whom does a user follow?
*/
function FollowingOperation(params, data) {
  Operation.call(this);  
  this.queryClass = Follow;
  this.queryParams = { "follower" : params[0] };
  this.run = this.buildQueryAndPresentResults;
}
FollowingOperation.signature = [ "get", "following" , "<user_name>" ];


/* Get the N most recent tweets
*/
function RecentTweetsOperation(params, data) {
  Operation.call(this);
  var limit = params && params[0] ? Number(params[0]) : 20;
  this.queryClass = Tweet; // use id > 0 to coerce a descending index scan on id
  this.queryParams =  {"zero" : 0, "order" : "desc", "limit" : limit};
  this.buildQuery = function(query, params) {
    query.where(query.id.gt(query.param("zero")));
  };
  this.run = this.buildQueryAndPresentResults;
}
RecentTweetsOperation.signature = [ "get", "tweets-recent" , "<count>" ];


/* Last 20 tweets from a user
*/
function TweetsByUserOperation(params, data) {
  Operation.call(this);
  this.queryClass = Tweet;
  this.queryParams = {"author_user_name" : params[0] ,
                      "order"            : "desc" ,
                      "limit"            : 20 };
  this.run = this.buildQueryAndPresentResults;
}
TweetsByUserOperation.signature = [ "get" , "tweets-by" , "<user_name>" ];


/* Common callback for @user and #hashtag queries 
*/
function fetchTweetsInBatch(operation, scanResults) {
  var batch, r;
  var resultData = [];

  function addTweetToResults(e, tweet) {
    if(tweet && ! e) {
      resultData.push(tweet);
    }
  }
  
  batch = operation.session.createBatch();
  if(scanResults.length) {
    r = scanResults.shift();
    while(r) {
      batch.find(Tweet, r.tweet_id, addTweetToResults);
      r = scanResults.shift();
    }
  }
  batch.execute(function(error) {
    if(error) { operation.onError(error); }
    else      { operation.setResult(resultData); operation.onComplete(); }
  });
}

/* Call fetchTweetsInBatch() closed over operation
*/
function fetchTweets(operation) {
  return function(error, results) {
    if(error) {
      operation.onError(error);
    } else {
      fetchTweetsInBatch(operation, results);
    }
  };
}

/* Last 20 tweets @user
*/
function TweetsAtUserOperation(params, data) {
  Operation.call(this);
  var tag = params[0];
  if(tag.charAt(0) == "@") {    tag = tag.substring(1);   }
  this.queryClass = Mention;
  this.queryParams = {"at_user" : tag, "order" : "desc" , "limit" : 20 };
  this.onQueryResults = fetchTweets(this);
  this.run = this.buildQueryAndPresentResults; 
}
TweetsAtUserOperation.signature = [ "get" , "tweets-at" , "<user_name>" ];


/* Last 20 tweets with hashtag
*/
function TweetsByHashtagOperation(params, data) {
  Operation.call(this);
  var tag = params[0];
  if(tag.charAt(0) == "#") {    tag = tag.substring(1);   }
  this.queryClass = HashtagEntry;
  this.queryParams = {"hashtag" : tag, "order" : "desc" , "limit" : 20 };
  this.onQueryResults = fetchTweets(this);  
  this.run = this.buildQueryAndPresentResults; 
}
TweetsByHashtagOperation.signature = [ "get" , "tweets-about", "<hashtag>" ];


/* The web server Operation is just slightly different; 
   it defines isServer = true, 
   and it defines a runServer() method, which takes the SessionFactory.
*/
function RunWebServerOperation(cli_params, cli_data) {
  var port = cli_params[0];
  var sessionFactory;
  this.isServer = true;
  
  function serverRequestLoop(request, response) {
    var params, data;

    function hangup(code) { 
      response.statusCode = code;
      response.end();
    }

    function runOperation() {
      var operation = parse_command(request.method, params, data);
      if(operation && ! operation.isServer) {
        operation.responder = new HttpOperationResponder(operation, response);
        sessionFactory.openSession().then(function(session) {
          onOpenSession(session, operation); });
      } else {
        hangup(400);
      }
    }

    data = "";
    params = url.parse(request.url).pathname.split("/");
    params.shift();
    
    request.setEncoding('utf8');
    function gatherData(chunk) {    data += chunk;    }
    request.on('data', gatherData);
    request.on('end', runOperation);
  }
  
  this.runServer = function(_sessionFactory) {
    sessionFactory = _sessionFactory;
    http.createServer(serverRequestLoop).listen(port);
    console.log("Server started on port", port);  
  };
}
RunWebServerOperation.signature = [ "start" , "server" , "<server_port_number>" ];


function HelpOperation(params, data) {
  Operation.call(this);
  
  this.run = function() {
    this.responder.write(operationMap.http_help);
    this.responder.close();
  };
}
HelpOperation.signature = [ "get" , "help" ];


function parse_command(method, params, data) {
  var verb, noun, opIdx, opConstructor, operation;
  verb = method.toLocaleLowerCase();
  noun = params.shift();
  operation = null;
  udebug.log("parse_command", verb, noun, params);
  if(operationMap.verbs[verb] && operationMap.verbs[verb][noun]) {
    opIdx = operationMap.verbs[verb][noun][0];
    opConstructor = allOperations[opIdx];
    if(opConstructor) {
      operation = {};
      opConstructor.call(operation, params, data);
    }
  }
  
  return operation;
}


function get_cmdline_args() { 
  var i, val, verb, operation;
  var cmdList = [];
  var usageMessage = 
    "Usage: node tweet {options} {command} {command arguments}\n" +
    "         -a <adapter>:  run using the named adapter (default: "+adapter+")\n"+
    "         -h or --help: print this message\n" +
    "         -d or --debug: set the debug flag\n" +
    "               --detail: set the detail debug flag\n" +
    "               -df <file>: enable debug output from <file>\n" +
    "         -E or --deployment <name>: use deployment <name> (default: test) \n" +
    "\n" +
    "  COMMANDS:\n" + operationMap.cli_help;
  
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
      case '-E':
      case '--deployment':
        deployment = process.argv[++i];
        break;
      default:
        cmdList.push(val);
    }
  }

  if(cmdList.length) {
    verb = cmdList.shift();
    /* Use final command line argument as "data" */    
    operation = parse_command(verb, cmdList, cmdList[cmdList.length-1]);
  }

  if(operation) {
    return operation;
  }
   
  console.log(usageMessage);
  process.exit(1);
}


/* Run a single operation specified on the command line
*/
function runCmdlineOperation(sessionFactory, operation) {
  udebug.log("runCmdlineOperation");
  if(operation.isServer) {
    operation.runServer(sessionFactory);
  }
  else {
    operation.responder = new ConsoleOperationResponder(operation);
    sessionFactory.openSession().then(function(session) {
      onOpenSession(session, operation); 
    }, console.trace);
  }
}

var allOperations = [ 
  AddUserOperation,
  LookupUserOperation,
  DeleteUserOperation,
  InsertTweetOperation,
  ReadTweetOperation,
  DeleteTweetOperation,
  FollowOperation,
  FollowersOperation, 
  FollowingOperation,
  TweetsByUserOperation,
  TweetsAtUserOperation,
  TweetsByHashtagOperation,
  RecentTweetsOperation,
  RunWebServerOperation,
  HelpOperation
];

function prepareOperationMap() {
  var idx, sig, verb, data;
  operationMap = {};
  operationMap.cli_help = "";
  operationMap.http_help = "";
  operationMap.verbs = { "get":{}, "post":{}, "put":{}, "delete":{} , "start":{} };
  
  for(idx in allOperations) {
    if(allOperations.hasOwnProperty(idx)) {
      sig = allOperations[idx].signature;
      operationMap.verbs[sig[0]][sig[1]] = [ idx, sig[2], sig[3] ];
      operationMap.cli_help += "         " + sig.join(" ") + "\n";
      verb = sig.shift().toLocaleUpperCase();
      data = sig[2];
      operationMap.http_help += verb +" /"+ sig[0] +"/"+ sig[1];
      operationMap.http_help += data ? data + "\n" : "\n";
    }
  }
}

// *** Main program starts here ***

/* Global Variable Declarations */
var mappings, dbProperties, operation, authorMapping;

prepareOperationMap();

/* This may have the side effect of changing the adapter */
operation = get_cmdline_args();

/* Connection Properties */
dbProperties = new jones.ConnectionProperties(adapter, deployment);

mappings = [];   // an array of mapped constructors

// Create a TableMapping for Author
authorMapping = new jones.TableMapping('author');
authorMapping.mapField("user_name");
authorMapping.mapField("full_name");
authorMapping.mapField("tweet_count");
mappings.push(authorMapping.applyToClass(Author));

// Map other SQL Tables to JS Constructors using default mappings
mappings.push(new jones.TableMapping('tweet').applyToClass(Tweet));
mappings.push(new jones.TableMapping('hashtag').applyToClass(HashtagEntry));
mappings.push(new jones.TableMapping('follow').applyToClass(Follow));
mappings.push(new jones.TableMapping('mention').applyToClass(Mention));

function run(sessionFactory) {
  runCmdlineOperation(sessionFactory, operation);
}

jones.connect(dbProperties, mappings).then(run, console.trace);
