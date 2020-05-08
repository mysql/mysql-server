define(["dojo/has!host-node?./node/fs:./rhino/fs"], function(result){
	// Code to strip BOM
	var origReadFileSync = result.readFileSync;
	result.readFileSync = function(filename, encoding) {
		var text = origReadFileSync(filename, encoding);
		if(encoding == "utf8"){
			text = text.replace(/^\uFEFF/, ''); // remove BOM
		}
		return text;
	};

	var origReadFile = result.readFile;
	result.readFile = function(filename, encoding, cb){
		origReadFile(filename, encoding, function(err, text){
			if(text && encoding == "utf8"){
				text = text.replace(/^\uFEFF/, ''); // remove BOM
			}
			cb(err, text);
		});
	};

	return result;
});
