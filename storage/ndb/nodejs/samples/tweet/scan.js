
/* This script provides an example of the Jones Query API. 
   In this example, we query the tweet table for posts 
   by a particular author, and apply a sort and limit
   on the query.
*/

"use strict";
var jones = require("database-jones");

if (process.argv.length < 3 ) {
  console.log("usage: node scan <author> [limit] [order]");
  process.exit(1);
}

/* new ConnectionProperties(adapter, deployment)
   see find.js for more information about ConnectionProperties
*/
var connectionProperties = new jones.ConnectionProperties("ndb", "test"),
    queryTerm = process.argv[2],   //  node scan.js <author> [limit] [order]
    limit = Number(process.argv[3]) || 20,
    order = (process.argv[4] == "asc" ? "asc" : "desc");


jones.openSession(connectionProperties).
  then(function(session) {
    return session.createQuery("tweet");
  }).
  then(function(query) {
    /* Here we can define query conditions.
       For more details see API-documentation/Query
    */
    query.where(query.author_user_name.eq(queryTerm));   // author == x
    // query.where(query.author_user_name.ne(queryTerm));   // author != x
    // query.where(query.author_user_name.gt(queryTerm));   // author > x
    // query.where(query.id.lt(queryTerm));                 // id < x

    /* Then execute the query, using limit & order parameters.
    */
    return query.execute({ "limit" : limit, "order" : order });
  }).
  then(console.log, console.trace).  // log the result or error
  then(jones.closeAllOpenSessionFactories);  // disconnect
