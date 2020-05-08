/*jshint white:false */
define([
	"../../buildControl",
	"dojo/has!host-node?./sendJob:",
	"../../fs",
	"dojo/has"
], function(bc, sendJob, fs, has){
	if(has("host-node")){
		return function(resource, text, copyright, optimizeSwitch/*, callback*/){
			copyright = copyright || "";
			sendJob(resource.dest + ".uncompressed.js", resource.dest, optimizeSwitch, copyright);
			return 0;
		};
	}

	if(has("host-rhino")){
		var sscompile = function(text, dest, optimizeSwitch, copyright){
			/*jshint rhino:true */
			/*global Packages:false */
			// decode the optimize switch
			var
				options = optimizeSwitch.split("."),
				comments = 0,
				keepLines = 0,
				strip = null;
			while(options.length){
				switch(options.pop()){
				case "normal":
					strip = "normal";
					break;
				case "warn":
					strip = "warn";
					break;
				case "all":
					strip = "all";
					break;
				case "keeplines":
					keepLines = 1;
					break;
				case "comments":
					comments = 1;
					break;
				}
			}

			//Use rhino to help do minifying/compressing.
			var context = Packages.org.mozilla.javascript.Context.enter();
			try{
				// Use the interpreter for interactive input (copied this from Main rhino class).
				context.setOptimizationLevel(-1);

				if(comments){
					//Strip comments
					var script = context.compileString(text, dest, 1, null);
					text = new String(context.decompileScript(script, 0));

					//Replace the spaces with tabs.
					//Ideally do this in the pretty printer rhino code.
					text = text.replace(/	 /g, "\t");
				}else{
					//Apply compression using custom compression call in Dojo-modified rhino.
					text = new String(Packages.org.dojotoolkit.shrinksafe.Compressor.compressScript(text, 0, 1, strip));
					if(!keepLines){
						text = text.replace(/[\r\n]/g, "");
					}
				}
			}finally{
				Packages.org.mozilla.javascript.Context.exit();
			}
			return copyright + "//>>built" + bc.newline + text;
		};

		return function(resource, text, copyright, optimizeSwitch, callback){
			bc.log("optimize", ["module", resource.mid]);
			copyright = copyright || "";
			try{
				var result = sscompile(text, resource.dest, optimizeSwitch, copyright);

				fs.writeFile(resource.dest, result, resource.encoding, function(err){
					if(err){
						bc.log("optimizeFailedWrite", ["filename", result.dest]);
					}
					callback(resource, err);
				});
			}catch(e){
				bc.log("optimizeFailed", ["module identifier", resource.mid, "exception", e + ""]);
				callback(resource, 0);
			}
			return callback;
		};
	}

	throw new Error("Unknown host environment: only nodejs and rhino are supported by shrinksafe optimizer.");
});