
var dlo = require("../build/Release/common/debug_dlopen");

var status = dlo.debug_dlopen("../build/Release/common/common_library.node");

console.log(status);


var db = require("../build/Release/common/common_library");

console.dir(db);

