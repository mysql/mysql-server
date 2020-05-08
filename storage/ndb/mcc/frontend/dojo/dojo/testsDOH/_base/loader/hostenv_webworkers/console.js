// summary:
//		Console polyfill for webworkers.
// description:
//		Webworkers do not have access to the console as of writing (except in Chrome).  This polyfills
//		the console by passing messages back to the browser window for it to reflect to the console.
//		This should make debugging of test failure a bit easier.

if(!self.console){
	console = {
		_sendMessage: function(type, message){
			self.postMessage({
				type: "console",
				consoleType: type,
				value: message
			});
		},
		log: function(message){
			console._sendMessage("log", message);
		},
		error: function(message){
			console._sendMessage("error", message);
		},
		warn: function(message){
			console._sendMessage("warn", message);
		},
		info: function(message){
			console._sendMessage("info", message);
		}
	}
}