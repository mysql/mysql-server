
/* This script provides an example of the Jones Projection API.

   Functionally it is identical to both "node tweet.js get tweets-by <author>"
   and to "node scan.js <author>". It produces a list of tweets by
   a particular user.
   
   However, where scan.js works by using a Query to scan the tweet table,
   join.js works by describing a one-to-many Projection from Author to
   Tweet, and then executing a find() on the Projection.
   
   In Jones, find() only returns a single result. In this case the result is
   a single instance of an Author.  However, that Author will have an array 
   of Tweets.

   Expressed in terms of SQL, scan.js is like "SELECT FROM tweet" using an
   ordered index, while join.js is like "SELECT FROM author" on primary key
   with "JOIN tweet on tweet.author = author.user_name".

   There are two steps to this. First, TableMappings describe the relationships
   of JavaScript objects to the two SQL tables and to each other. Then, a
   Projection describes the desired shape of the result, referring to the mapped
   constructors.

   NOTE - 3 Oct 2015 - due to a known bug, this example is currently working
   correctly with the "mysql" adapter but not with the "ndb" adapter.
*/

"use strict";
var jones = require("database-jones");


/* Constructors for application objects */
function Author() { }

function Tweet() { }


/*  TableMappings describe the structure of the data. */
var authorMapping = new jones.TableMapping("author");
authorMapping.applyToClass(Author);
authorMapping.mapOneToMany(
  { fieldName:    "tweets",      // field in the Author object
    target:       Tweet,         // mapped constructor
    targetField:  "author"       // target join field
  });

var tweetMapping = new jones.TableMapping("tweet");
tweetMapping.applyToClass(Tweet);

tweetMapping.mapManyToOne(
  { fieldName:    "author",      // field in the Tweet object
    target:       Author,        // mapped constructor
    foreignKey:   "author_fk"    // SQL foreign key relationship
  });



/* 
   Projections describe the structure to be returned from find().
*/
var tweetProjection = new jones.Projection(Tweet);
tweetProjection.addFields(["id", "message","date_created"]);

var authorProjection = new jones.Projection(Author);
authorProjection.addRelationship("tweets", tweetProjection);
authorProjection.addFields(["user_name", "full_name"]);


/* This script takes one argument, the user name.  e.g.:
   "node join.js uncle_claudius"
*/
if (process.argv.length !== 3) {
  console.err("Usage: node join.js <user_name>\n");
  process.exit(1);
}
var find_key = process.argv[2];


/* The rest of this example looks like find.js, 
   only using find() with a projection rather than a table name.
*/

jones.openSession(new jones.ConnectionProperties("mysql", "test")).
  then(function(session) {
    return session.find(authorProjection, find_key);
  }).
  then(console.log, console.trace).    // log the result or error
  then(jones.closeAllOpenSessionFactories).  // disconnect
  then(process.exit, console.trace);
