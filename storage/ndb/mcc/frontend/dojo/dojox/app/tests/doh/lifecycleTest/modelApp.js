require(["dojo/_base/window","dojox/app/main", "dojox/json/ref", "dojo/text!./config.json"],
function(win, Application, jsonRef, config){
	Application(jsonRef.fromJson(config));
});
