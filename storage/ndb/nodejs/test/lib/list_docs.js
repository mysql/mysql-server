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

var doc_parser = require("./doc_parser.js");

var list_file;

function main() { 
  var i, suite, file;
  var doc_dir;
  suite = process.argv[2] || "api";
  file  = process.argv[3];

  doc_dir = global[suite + "_doc_dir"];
  
  if(file) {
    list_file(doc_dir, file);
  }
  else {
    files = fs.readdirSync(doc_dir);
    while(file = files.pop()) {
      list_file(doc_dir, file);
    }
  }
}

function list_file(dir, file) {
  var list, i;
  console.log(file,":");
  
  list = doc_parser.listFunctions(path.join(dir, file));

  if(list._found_constructor) {
    console.log("  * ", list._found_constructor);
  }
  for(i = 0 ; i < list.length ; i++) {
    console.log("    ", list[i]);
  }
}

main();
