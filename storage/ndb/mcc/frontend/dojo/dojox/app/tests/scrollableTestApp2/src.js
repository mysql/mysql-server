require(["dojox/app/main", "dojox/json/ref", "dojo/sniff"],
	function(Application, json, has){

	var configurationFile = "./config.json";

	require(["dojo/text!"+configurationFile], function(configJson){
		var config = json.fromJson(configJson);
		var width = window.innerWidth || document.documentElement.clientWidth;
		if(width <= 600){
			has.add("phone", true);
		}
		has.add("ie9orLess", has("ie") && (has("ie") <= 9));
		Application(config);
	});
});
