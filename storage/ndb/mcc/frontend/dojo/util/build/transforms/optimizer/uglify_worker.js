function factory(uglify, fs){
	if(!uglify){
		throw new Error("Unknown host environment: only nodejs is supported by uglify optimizer.");
	}
	if(uglify.minify){
		//uglify2, provide a uglify-1 compatible uglify function
		var UglifyJS = uglify;
		uglify = function(code, options, dest, useSourceMaps){
			//parse
			var ast = UglifyJS.parse(code, options);
			ast.figure_out_scope();

			//by default suppress warnings from uglify2
			var compress_options = options.compress_options || {};
			if(!('warnings' in compress_options)){
				compress_options.warnings = false;
			}
			var compressor = UglifyJS.Compressor(compress_options);
			compressed_ast = ast.transform(compressor);
			compressed_ast.figure_out_scope();

			//mangle
			compressed_ast.compute_char_frequency();
			compressed_ast.mangle_names();

			var gen_options = options.gen_options || {};
			if (useSourceMaps) {
				var source_map = gen_options.source_map || {};
				source_map.file = options.filename.split("/").pop();
				// account for the //>> built line
				source_map.dest_line_diff = 1;
				gen_options.source_map = UglifyJS.SourceMap(source_map);
			}

			var output = compressed_ast.print_to_string(gen_options);

			if (useSourceMaps) {
				output += "//# sourceMappingURL=" + dest.split("/").pop() + ".map";
				fs.writeFile(dest + ".map", gen_options.source_map.toString(), "utf-8", function() {});
			}

			return output;
		}
	}
	return uglify;
}

if(global.define){
	//loaded by dojo AMD loader
	define(["dojo/has!host-node?dojo/node!uglify-js:", "../../fs"], factory);
}else{
	//loaded in a node sub process
	try{
		var uglify = require("uglify-js");
		var fs = require("fs");
	}catch(e){}
	uglify = factory(uglify, fs);
	process.on("message", function(data){
		var result = "", error = "";
		try{
			var result = uglify(data.text, data.options, data.dest, data.useSourceMaps);
		}catch(e){
			error = e.toString() + " " + e.stack;
		}
		process.send({text: result, dest: data.dest, error: error});
	});
}
