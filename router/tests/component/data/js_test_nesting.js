var path = require("path");

// the test-modules are not in the standard paths
//
// the first one should be local_modules/
// test_modules/ is just right next to it
module.paths.push(path.join(module.paths[0], "..", "test_modules"));

require("test-require-nesting-1")
