define(["../fileHandleThrottle"], function(fht){
	var fs = require.nodeRequire("fs");
	return {
		statSync:fs.statSync,
		mkdirSync:fs.mkdirSync,
		readFileSync:fs.readFileSync,
		writeFileSync:fs.writeFileSync,
		readdirSync:fs.readdirSync,

		copyFile:function(src, dest, cb){
			// Use no encoding, as the file may be text or binary.
			fht.enqueue(function() {
				fs.readFile(src, undefined, function (err, contents) {
					if (err) {
						fht.release();
						cb(err);
					} else {
						fs.writeFile(dest, contents, undefined, cb);
						fht.release();
					}
				});
			});
		},

		readFile:function(filename, encoding, cb){
			fht.enqueue(function(){
				fs.readFile(filename, encoding, function(code){
					fht.release();
					cb.apply(null, arguments);
				});
			});
		},

		writeFile:function(filename, contents, encoding, cb){
			fht.enqueue(function(){
				fs.writeFile(filename, contents, encoding, function(code){
					fht.release();
					cb.apply(null, arguments);
				});
			});
		}
	};
});
