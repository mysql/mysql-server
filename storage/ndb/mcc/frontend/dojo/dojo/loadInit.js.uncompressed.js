//>>built
define("dojo/loadInit", ["./_base/loader"], function(loader){
	return {
		dynamic:0,
		normalize:function(id){return id;},
		load:loader.loadInit
	};
});
