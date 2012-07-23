
var path = require("path"),
    fs = require("fs");
var parent = path.dirname(__dirname);


global.path            = path;
global.fs              = fs;
global.assert          = require("assert");

global.driver_dir      = __dirname;
global.adapter_dir     = path.join(parent, "Adapter");
global.spi_module      = path.join(adapter_dir, "impl", "SPI.js");
global.api_module      = path.join(adapter_dir, "api", "mynode.js");

global.harness         = require(path.join(__dirname, "harness"));
global.mynode          = require(api_module);

global.debug           = false;
global.exit            = false;

/* Connection properties for use in running the test suite 
*/
global.test_conn_properties = {};
