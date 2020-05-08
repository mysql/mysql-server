/*jshint rhino:true white:false */
/*global Packages:false com:false */
function writeFile(filename, contents, encoding, cb) {
	if (arguments.length === 3 && typeof encoding !== "string") {
		cb = encoding;
		encoding = 0;
	}
	var
		outFile = new java.io.File(filename),
		outWriter;
	if(encoding){
		outWriter = new java.io.OutputStreamWriter(new java.io.FileOutputStream(outFile), encoding);
	}else{
		outWriter = new java.io.OutputStreamWriter(new java.io.FileOutputStream(outFile));
	}

	var os = new java.io.BufferedWriter(outWriter);
	try{
		os.write(contents);
	}finally{
		os.close();
	}
	if (cb) {
		cb(0);
	}
}

var built = "//>>built\n";

function sscompile(src, dest, optimizeSwitch, copyright){
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
	var context = Packages.org.mozilla.javascript.Context.enter(),
		text;
	try{
		// Use the interpreter for interactive input (copied this from Main rhino class).
		context.setOptimizationLevel(-1);

		text = readFile(src, "utf-8");
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
	writeFile(dest, copyright + built + text, "utf-8");
}

var JSSourceFilefromCode, closurefromCode, compiler, jscomp = 0;
function ccompile(src, dest, optimizeSwitch, copyright, optimizeOptions, useSourceMaps){
	optimizeOptions = optimizeOptions || {};
	if(!jscomp){
		JSSourceFilefromCode=java.lang.Class.forName("com.google.javascript.jscomp.SourceFile").getMethod("fromCode", [java.lang.String, java.lang.String]);
		closurefromCode = function(filename,content){
			return JSSourceFilefromCode.invoke(null,[filename,content]);
		};
		jscomp = com.google.javascript.jscomp;
	}

	//Fake extern required with 'Symbol' defined
	var externSourceFile = closurefromCode("fakeextern.js", "function Symbol(description) {}");

	//Set up source input
	// it is possible dest could have backslashes on windows (particularly with cygwin)
	var destFilename = dest.match(/^.+[\\\/](.+)$/)[1],
		jsSourceFile = closurefromCode(destFilename + ".uncompressed.js", String(readFile(src, "utf-8")));

	//Set up options
	var options = new jscomp.CompilerOptions();
	var set;

	// Set the closure optimization level, use simple optimizations by default
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
	var mapTag = useSourceMaps ? ("\n//# sourceMappingURL=" + destFilename + ".map") : "";
	compiler = new Packages.com.google.javascript.jscomp.Compiler(Packages.java.lang.System.err);


	compiler.compile(externSourceFile, jsSourceFile, options);
	writeFile(dest, copyright + built + compiler.toSource() + mapTag, "utf-8");

	if(useSourceMaps){
		var sourceMap = compiler.getSourceMap();
		sourceMap.setWrapperPrefix(copyright + built);
		var sb = new java.lang.StringBuffer();
		sourceMap.appendTo(sb, destFilename);
		writeFile(dest + ".map", sb.toString(), "utf-8");
	}
}

function shutdownClosureExecutorService(){
	try{
		var compilerClass = java.lang.Class.forName("com.google.javascript.jscomp.Compiler");
		var compilerExecutorField = compilerClass.getDeclaredField("compilerExecutor");
		compilerExecutorField.setAccessible(true);
		var compilerExecutor = compilerExecutorField.get(compiler);
		compilerClass = compilerExecutor.getClass();
		compilerExecutorField = compilerClass.getDeclaredField("compilerExecutor");
		compilerExecutorField.setAccessible(true);
		compilerExecutor = compilerExecutorField.get(compilerExecutor);
		compilerExecutor.shutdown();
	}catch (e){
		print(e);
		if("javaException" in e){
			e.javaException.printStackTrace();
		}
	}
}

var
	console = new java.io.BufferedReader(new java.io.InputStreamReader(java.lang.System["in"])),
	readLine = function(){
		// the + "" convert to a Javascript string
		return console.readLine() + "";
	},
	src,
	dest,
	optimizeSwitch;

while(1){
	src = readLine();
	if(src === "."){
		break;
	}
	dest = readLine();
	optimizeSwitch = readLine();
	var options = eval("(" + readLine() + ")");
	print(dest + ":");
	var start = (new Date()).getTime(),
		exception = "";
	try{
		if(/closure/.test(optimizeSwitch)){
			ccompile(src, dest, optimizeSwitch, options.copyright, options.options, options.useSourceMaps);
		}else{
			sscompile(src, dest, optimizeSwitch, options.copyright);
		}
	}catch(e){
		exception = ". OPTIMIZER FAILED: " + e;
	}
	print("Done (compile time:" + ((new Date()).getTime()-start)/1000 + "s)" + exception);
}

if (jscomp) {
	shutdownClosureExecutorService();
}
