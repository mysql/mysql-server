require(["dojo/_base/window","dojox/app/main", "dojox/json/ref", "dojo/text!./config.json", "dojo/sniff"],
function(win, Application, jsonRef, config, has){
	win.global.modelApp = {};
	modelApp.listData = { 
		identifier: "id",
		'items':[{
			"id": "item1",
			"label": "Chad Chapman",
			"rightIcon":"mblDomButtonBlueCircleArrow",
			"First": "Chad",
			"Last": "Chapman",
			"Location": "CA",
			"Office": "1278",
			"Email": "c.c@test.com",
			"Tel": "408-764-8237",
			"Fax": "408-764-8228"
		}, {
			"id": "item2",
			"label": "Irene Ira",
			"rightIcon":"mblDomButtonBlueCircleArrow",
			"First": "Irene",
			"Last": "Ira",
			"Location": "NJ",	
			"Office": "F09",
			"Email": "i.i@test.com",
			"Tel": "514-764-6532",
			"Fax": "514-764-7300"
		}, {
			"id": "item3",
			"label": "John Jacklin",
			"rightIcon":"mblDomButtonBlueCircleArrow",
			"First": "John",
			"Last": "Jacklin",
			"Location": "CA",
			"Office": "6701",
			"Email": "j.j@test.com",
			"Tel": "408-764-1234",
			"Fax": "408-764-4321"
		}]
	};
	var cfg = jsonRef.fromJson(config);
	has.add("ie9orLess", has("ie") && (has("ie") <= 9));	
	cfg.controllers[2] = "dojox/app/controllers/Layout";		
	console.log("cfg.controllers[2] was set to Layout to force it to use Layout="+cfg.controllers[2]);	
	Application(cfg);
	
});
