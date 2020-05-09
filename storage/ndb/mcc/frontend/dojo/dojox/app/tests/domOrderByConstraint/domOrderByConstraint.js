require(["dojo/_base/window","dojox/app/main", "dojox/json/ref", "dojo/text!./config.json", "dojo/sniff"],
function(win, Application, jsonRef, config, has){
	win.global.domOrderByConstraint = {};
	domOrderByConstraint.names = {
		identifier: "id",
		items: [{
			"id": "1",
			"Serial": "360324",
			"First": "John",
			"Last": "Doe",
			"Email": "jdoe@us.ibm.com",
			"ShipTo": {
				"Street": "123 Valley Rd",
				"City": "Katonah",
				"State": "NY",
				"Zip": "10536"
			},
			"BillTo": {
				"Street": "17 Skyline Dr",
				"City": "Hawthorne",
				"State": "NY",
				"Zip": "10532"
			}
		}]
	};

	domOrderByConstraint.listData = {
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
	Application(cfg);

});
