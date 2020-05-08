require(["dojo/_base/window","dojox/app/main", "dojox/json/ref", "dojo/sniff"],
	function(win, Application, json, has){
	win.global.modelApp = {};
	modelApp.list = { 
		identifier: "label",
		'items':[]
	};


	var configurationFile = "./config.json";

	require(["dojo/text!"+configurationFile], function(configJson){
		var config = json.fromJson(configJson);
		var width = window.innerWidth || document.documentElement.clientWidth;
	//	if(width <= 600){  // for this test only use tablet mode...
	//		has.add("phone", true);
	//	}
		has.add("ie9orLess", has("ie") && (has("ie") <= 9));
		Application(config);
	});
});
