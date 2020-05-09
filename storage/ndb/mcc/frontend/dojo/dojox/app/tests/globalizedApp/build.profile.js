require(["dojox/app/build/buildControlApp"], function(bc){
});

var profile = {
	basePath: "..",
	releaseDir: "./globalizedApp/release",
	action: "release",
	cssOptimize: "comments",
	packages:[{
		name: "dojo",
		location: "../../../dojo"
	},{
		name: "dijit",
		location: "../../../dijit"
	},{
		name: "globalizedApp",
		location: "../../../dojox/app/tests/globalizedApp",
		destLocation: "./dojox/app/tests/globalizedApp"
	},{
		name: "dojox",
		location: "../../../dojox"
	}],
	layers: {
		"globalizedApp/globalizedApp": {
			include: [ "globalizedApp/index.html" ]
		}
	}
};



