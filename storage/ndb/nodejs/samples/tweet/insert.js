/* This script shows an example persist() operation using a table name and
   primary key, and working with callbacks. 
   
   For a similar example using promises rather than callbacks, see find.js
*/

"use strict";
var jones = require("database-jones");


/*  new ConnectionProperties(adapter, deployment)

    The first argument names a database backend, e.g. "ndb", "mysql", etc.

    The second argument names a "deployment" as defined in the file
    jones_deployments.js (found two directories up from this one).  The 
    preferred way to customize the host, username, password, schema, etc., 
    used for the database connection is to edit the deployments file.
*/
var connectionProperties = new jones.ConnectionProperties("ndb", "test");

function disconnectAndExit(status) {
  jones.closeAllOpenSessionFactories(function() {
    process.exit(status);
  });
}

/* handleError() exits if "error" is set, or otherwise simply returns.
*/
function handleError(error) {
  if(error) {
    console.trace(error);
    disconnectAndExit(1);
  }
}

/* node     find.js     table_name      JSON_object
   argv[0]  argv[1]     argv[2]         argv[3]             */
if (process.argv.length !== 4) {
  handleError("Usage: node insert <table> <JSON_object>\n");
}

var table_name = process.argv[2],
    object     = JSON.parse(process.argv[3]);

/* This version of openSession() takes three arguments:
     ConnectionProperties
     A table name, which will be validated upon connecting
     A callback which will receive (error, session)
*/
jones.openSession(connectionProperties, table_name, function(err, session) {
  handleError(err);

  /* The callback for persist() only gets one argument */
  session.persist(table_name, object, function(err) {
    handleError(err);
    console.log("Inserted: ", object);
    disconnectAndExit(0);
  });
});

