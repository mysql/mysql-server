define(["../../buildControl"], function(bc){
	if(bc.stripConsole){
		var consoleMethods = "assert|count|debug|dir|dirxml|group|groupEnd|info|profile|profileEnd|time|timeEnd|trace|log";
		if(bc.stripConsole === "warn"){
			consoleMethods += "|warn";
		}else if(bc.stripConsole === "all"){
			consoleMethods += "|warn|error";
		}
		// Match on "window.console" and plain "console" but not things like "myconsole" or "my.console"
		var stripConsoleRe = new RegExp("([^\\w\\.]|^)((window.)?console\\.(" + consoleMethods + ")\\s*\\()", "g");
		return function(text){
			return text.replace(stripConsoleRe, "$1 0 && $2");
		};
	}else{
		return function(text){
			return text;
		};
	}
});