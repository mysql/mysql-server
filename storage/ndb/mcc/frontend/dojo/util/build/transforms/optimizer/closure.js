define([
	"../../buildControl",
	"dojo/has!host-node?./sendJob:",
	"../../fs",
	"../../fileUtils",
	"./stripConsole",
	"dojo/_base/lang",
	"dojo/has"
], function(bc, sendJob, fs, fileUtils, stripConsole, lang, has){
	if(has("host-node")){
		return function(resource, text, copyright, optimizeSwitch, callback){
			copyright = copyright || "";
			if(bc.stripConsole){
				var tempFilename = resource.dest + ".consoleStripped.js";
				text = stripConsole(text);
				fs.writeFile(tempFilename, bc.newlineFilter(text, resource, "closureStripConsole"), resource.encoding, function(err){
					if(!err){
						sendJob(tempFilename, resource.dest, optimizeSwitch, copyright);
					}
					callback(resource, err);
				});
				return callback;
			}else{
				sendJob(resource.dest + ".uncompressed.js", resource.dest, optimizeSwitch, copyright);
				return 0;
			}
		};
	}

	if(has("host-rhino")){
		var JSSourceFilefromCode,
			closurefromCode,
			jscomp = 0,
			built = "//>>built" + bc.newline;

		var ccompile = function(text, dest, optimizeSwitch, copyright, useSourceMaps){
			/*jshint rhino:true */
			/*global com:false Packages:false */
			if(!jscomp){
				JSSourceFilefromCode = java.lang.Class.forName("com.google.javascript.jscomp.SourceFile").getMethod("fromCode", [ java.lang.String, java.lang.String ]);
				closurefromCode = function(filename,content){
					return JSSourceFilefromCode.invoke(null, [filename, content]);
				};
				jscomp = com.google.javascript.jscomp;
			}
			//Fake extern
			var externSourceFile = closurefromCode("fakeextern.js", "function Symbol(description) {}");

			//Set up source input
			var destFilename = dest.split("/").pop(),
				jsSourceFile = closurefromCode(destFilename + ".uncompressed.js", String(text));

			//Set up options
			var options = new jscomp.CompilerOptions();
			var optimizeOptions = bc.optimizeOptions || {};

			var FLAG_compilation_level = jscomp.CompilationLevel.SIMPLE_OPTIMIZATIONS;
			if (optimizeOptions.compilationLevel) {
				switch (optimizeOptions.compilationLevel.toUpperCase()) {
					case "WHITESPACE_ONLY": FLAG_compilation_level = jscomp.CompilationLevel.WHITESPACE_ONLY;
						break;
					case "SIMPLE_OPTIMIZATIONS": FLAG_compilation_level = jscomp.CompilationLevel.SIMPLE_OPTIMIZATIONS;
						break;
					case "ADVANCED_OPTIMIZATIONS": FLAG_compilation_level = jscomp.CompilationLevel.ADVANCED_OPTIMIZATIONS;
						break;
				}
			}
			FLAG_compilation_level.setOptionsForCompilationLevel(options);
			var FLAG_warning_level = jscomp.WarningLevel.DEFAULT;
			FLAG_warning_level.setOptionsForWarningLevel(options);

			// force this option to false to prevent overly aggressive code elimination (#18919)
			options.setDeadAssignmentElimination(false);

			for(var k in optimizeOptions){
				// Skip compilation level option
				if (k === 'compilationLevel') {
					continue;
				}

				// Set functions requiring Java Enum values
				if (k === 'languageIn') {
					options.setLanguageIn(jscomp.CompilerOptions.LanguageMode.fromString(optimizeOptions[k].toUpperCase()));
					continue;
				}

				if (k === 'languageOut') {
					options.setLanguageOut(jscomp.CompilerOptions.LanguageMode.fromString(optimizeOptions[k].toUpperCase()));
					continue;
				}

				if (k === 'variableRenaming') {
					switch (optimizeOptions[k].toUpperCase()) {
						case 'OFF': options.setVariableRenaming(jscomp.VariableRenamingPolicy.OFF);
							break;
						case 'LOCAL': options.setVariableRenaming(jscomp.VariableRenamingPolicy.LOCAL);
							break;
						case 'ALL': options.setVariableRenaming(jscomp.VariableRenamingPolicy.ALL);
							break;
					}
					continue;
				}

				if (k === 'propertyRenaming') {
					switch (optimizeOptions[k].toUpperCase()) {
						case 'OFF': options.setPropertyRenaming(jscomp.PropertyRenamingPolicy.OFF);
							break;
						case 'ALL_UNQUOTED': options.setPropertyRenaming(jscomp.PropertyRenamingPolicy.ALL_UNQUOTED);
							break;
					}
					continue;
				}

				if (k === 'checkGlobalThisLevel') {
					switch (optimizeOptions[k].toUpperCase()) {
						case 'ERROR': options.setCheckGlobalThisLevel(jscomp.CheckLevel.ERROR);
							break;
						case 'WARNING': options.setCheckGlobalThisLevel(jscomp.CheckLevel.WARNING);
							break;
						case 'OFF': options.setCheckGlobalThisLevel(jscomp.CheckLevel.OFF);
							break;
					}
					continue;
				}

				if (k === 'uselessCode') {
					var uselessCode = jscomp.DiagnosticGroups.CHECK_USELESS_CODE;
					switch (optimizeOptions[k].toUpperCase()) {
						case 'ERROR': options.setWarningLevel(uselessCode, jscomp.CheckLevel.ERROR);
							break;
						case 'WARNING': options.setWarningLevel(uselessCode, jscomp.CheckLevel.WARNING);
							break;
						case 'OFF': options.setWarningLevel(uselessCode, jscomp.CheckLevel.OFF);
							break;
					}
					continue;
				}

				if (k in {
					'removeUnusedVariables': 1,
					'inlineVariables': 1,
					'inlineFunctions': 1
				}) {
					set = "set" + k.charAt(0).toUpperCase() + k.substr(1);
					switch (optimizeOptions[k].toUpperCase()) {
						case 'ALL': options[set](jscomp.CompilerOptions.Reach.ALL);
							break;
						case 'LOCAL_ONLY': options[set](jscomp.CompilerOptions.Reach.LOCAL_ONLY);
							break;
						case 'NONE': options[set](jscomp.CompilerOptions.Reach.NONE);
							break;
					}
					continue;
				}

				// Set functions with boolean values
				if (k === 'dependencySorting') {
					options.dependencyOptions.setDependencySorting(optimizeOptions[k]);
					continue;
				}

				if (k in {
					'assumeClosuresOnlyCaptureReferences': 1,
					'closurePass': 1,
					'coalesceVariableNames': 1,
					'collapseAnonymousFunctions': 1,
					'collapseProperties': 1,
					'collapseVariableDeclarations': 1,
					'computeFunctionSideEffects': 1,
					'crossModuleCodeMotion': 1,
					'crossModuleMethodMotion': 1,
					'deadAssignmentElimination': 1,
					'devirtualizePrototypeMethods': 1,
					'extractPrototypeMemberDeclarations': 1,
					'flowSensitiveInlineVariables': 1,
					'foldConstants': 1,
					'inlineConstantVars': 1,
					'optimizeArgumentsArray': 1,
					'removeDeadCode': 1,
					'removeUnusedClassProperties': 1,
					'removeUnusedPrototypeProperties': 1,
					'removeUnusedPrototypePropertiesInExterns': 1,
					'rewriteFunctionExpressions': 1,
					'smartNameRemoval': 1
				}) {
					set = 'set' + k.charAt(0).toUpperCase() + k.substr(1);
					options[set](optimizeOptions[k]);
					continue;
				}

				// Otherwise assign value to key
				options[k] = optimizeOptions[k];
			}
			// Must have non-null path to trigger source map generation, also fix version
			options.setSourceMapOutputPath("");
			options.setSourceMapFormat(jscomp.SourceMap.Format.V3);
			if(optimizeSwitch.indexOf(".keeplines") !== -1){
				options.prettyPrint = true;
			}


			//Prevent too-verbose logging output
			Packages.com.google.javascript.jscomp.Compiler.setLoggingLevel(java.util.logging.Level.SEVERE);

			// Run the compiler
			// File name and associated map name
			var mapTag = useSourceMaps ? (bc.newline + "//# sourceMappingURL=" + destFilename + ".map") : "";
			var compiler = new Packages.com.google.javascript.jscomp.Compiler(Packages.java.lang.System.err);
			compiler.compile(externSourceFile, jsSourceFile, options);
			var result = copyright + built + compiler.toSource() + mapTag;

			if(useSourceMaps){
				var sourceMap = compiler.getSourceMap();
				sourceMap.setWrapperPrefix(copyright + built);
				var sb = new java.lang.StringBuffer();
				sourceMap.appendTo(sb, destFilename);
				fs.writeFile(dest + ".map", sb.toString(), "utf-8");
			}

			return result;
		};

		return function(resource, text, copyright, optimizeSwitch, callback){
			bc.log("optimize", ["module", resource.mid]);
			copyright = copyright || "";
			try{
				var result = ccompile(stripConsole(text), resource.dest, optimizeSwitch, copyright, bc.useSourceMaps);
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

	throw new Error("Unknown host environment: only nodejs and rhino are supported by closure optimizer.");
});
