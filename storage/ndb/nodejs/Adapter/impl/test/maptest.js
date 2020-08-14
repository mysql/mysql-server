/*
   Copyright (c) 2013, 2020, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

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

