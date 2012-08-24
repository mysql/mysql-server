require("../../api/mynode");

var dbdl = require("../build/Release/common/debug_dlopen");

console.log(dbdl.debug_dlopen("../build/Release/ndb/ndb_adapter.node"));

var ada = require("../build/Release/ndb/ndb_adapter.node");

console.dir(ada);
 

