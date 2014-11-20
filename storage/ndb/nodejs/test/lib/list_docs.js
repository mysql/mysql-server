
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
