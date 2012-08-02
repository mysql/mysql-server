console.log("line 1");
var mapper = require("../../build/Release/common/test/mapper.node");

console.log("line 4");
console.log("%d ", mapper.whatnumber(3, "cowboy"));

console.log("line 7");
var p = new mapper.Point(1, 6);
console.log("p:");
console.dir(p);
// process.exit();

console.log("line 10");
console.log("quardrant: %d", p.quadrant());

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


