define("dojo/_firebug/firebug", [], function(){

	// module:
	//		dojo/_firebug/firebug
	// summary:
	//		This file used to contain code for a "firebug lite" console.  Now it just fixes IE's console.log()
	//		command to insert spaces.


	var isNewIE = (/Trident/.test(window.navigator.userAgent));
	if(isNewIE){
		// Fixing IE's console
		// IE doesn't insert space between arguments. How annoying.
		var calls = ["log", "info", "debug", "warn", "error"];
		for(var i=0;i<calls.length;i++){
			var m = calls[i];
			if(!console[m] ||console[m]._fake){
				// IE9 doesn't have console.debug method, a fake one is added later
				continue;
			}
			var n = "_"+calls[i];
			console[n] = console[m];
			console[m] = (function(){
				var type = n;
				return function(){
					console[type](Array.prototype.join.call(arguments, " "));
				};
			})();
		}
		// clear the console on load. This is more than a convenience - too many logs crashes it.
		// If closed it throws an error
		try{ console.clear(); }catch(e){}
	}

	// There used to be code here for adding a "firebug lite" console, but it's no longer needed on any
	// of the browsers we support, because they all have built-in consoles.

});
