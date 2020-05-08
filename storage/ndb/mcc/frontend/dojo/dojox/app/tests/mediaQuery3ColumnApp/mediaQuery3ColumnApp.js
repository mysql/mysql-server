require(["dojo/_base/window","dojox/app/main", "dojox/json/ref", "dojo/text!./config.json", "dojo/sniff"],
	function(win, Application, jsonRef, config, has){

	var small = 560;

	// large > 860 medium <= 860  small <= 560 
	var isSmall = function(){				
		var width = window.innerWidth || document.documentElement.clientWidth;
		return width <= small;

	};

	
	var cfg = jsonRef.fromJson(config);
	has.add("ie9orLess", has("ie") && (has("ie") <= 9));
	has.add("isInitiallySmall", isSmall());
	Application(cfg);
	
});
