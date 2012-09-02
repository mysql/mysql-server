var dbdl = require("../build/Release/test/debug_dlopen");

function test(f) { 
  var status;
  console.log(f);
  status = dbdl.debug_dlopen("../build/Release/" + f);
  console.log(status);
  if(status === "OK") {
    var module = require("../build/Release/" + f);
    console.dir(module);
  }
}

test("ndb_adapter.node");
test("test/mapper.node");
test("test/outermapper.node");

