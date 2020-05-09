require(["dojox/app/build/buildControlApp"], function(bc){
});

var profile = {
	basePath: "..",
	releaseDir: "./layoutApp/release",
	action: "release",
	cssOptimize: "comments",
/*	multipleAppConfigLayers: true,*/
	packages:[{
		name: "dojo",
		location: "../../../dojo"
	},{
		name: "dijit",
		location: "../../../dijit"
	},{
		name: "layoutApp",
		location: "../../../dojox/app/tests/layoutApp",
		destLocation: "./dojox/app/tests/layoutApp"
	},{
		name: "dojox",
		location: "../../../dojox"
	}],
	layers: {
		"layoutApp/layoutApp": {
			include: [ "layoutApp/index.html" ]
		}
	}
};



