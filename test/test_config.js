
var path = require("path"),
    fs = require("fs"),
    assert = require("assert"),
    util = require("util");

var parent = path.dirname(__dirname);

global.path            = path;
global.fs              = fs;
global.assert          = assert;
global.util            = util;

global.driver_dir      = __dirname;
global.suites_dir      = driver_dir;
global.adapter_dir     = path.join(parent, "Adapter");
global.build_dir       = path.join(adapter_dir, "impl", "build", "Release");

global.spi_module      = path.join(adapter_dir, "impl", "SPI.js");
global.api_module      = path.join(adapter_dir, "api", "mynode.js");
global.udebug_module   = path.join(adapter_dir, "api", "unified_debug.js");

global.harness         = require(path.join(__dirname, "harness"));
global.mynode          = require(api_module);
global.udebug          = require(udebug_module);

global.debug           = false;
global.adapter         = "ndb";

/* Connection properties for use in running the test suite 
*/
global.test_conn_properties = {};
