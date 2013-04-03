console.log("line 1");
var udebug = require("../../api/unified_debug.js").getLogger("maptest.js");
var mapper = require("../build/Release/test/mapper.node");
var dmapper = require("../build/Release/test/outermapper.node");

udebug.on();
udebug.all_files();

console.log("line 4");
console.log("%d ", mapper.whatnumber(3, "cowboy"));

console.log("line 7");
var p = new mapper.Point(1, 6);
console.log("p:");
console.dir(p);
// process.exit();

console.log("line 10");
console.log("quadrant: %d", p.quadrant());

console.log("line 13");
var c = new mapper.Circle(p, 2.5);
console.log("c:");
console.dir(c);
// process.exit();

console.log("line 16");
console.log("area: %d", c.area());

var d = c;
console.dir(d);
console.log("d area: %d", d.area());

var x = dmapper.doubleminus(4);
console.log("doubleminus 4: %d", x);

