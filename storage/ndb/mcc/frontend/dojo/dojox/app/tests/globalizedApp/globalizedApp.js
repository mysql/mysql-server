require(["dojox/app/main", "dojox/json/ref", "dojo/text!./config.json", "dojo/sniff"],
function(Application, jsonRef, config, has){
	var cfg = jsonRef.fromJson(config);
	has.add("ie9orLess", has("ie") && (has("ie") <= 9));
	Application(cfg);
});
