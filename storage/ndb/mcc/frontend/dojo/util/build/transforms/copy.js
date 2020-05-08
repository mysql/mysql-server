define([
	"../buildControl",
	"../fileUtils",
	"../fs",
	"dojo/has"
], function(bc, fileUtils, fs, has) {

	function copyFileWithFs(src, dest, cb) {
		if (has("is-windows")) {
			src = fileUtils.normalize(src);
			dest = fileUtils.normalize(dest);
		}
		fs.copyFile(src, dest, cb);
	}

	return function(resource, callback) {
		fileUtils.ensureDirectoryByFilename(resource.dest);
		copyFileWithFs(resource.src, resource.dest, function(code){
			callback(resource, code);
		});
		return callback;
	};
});
