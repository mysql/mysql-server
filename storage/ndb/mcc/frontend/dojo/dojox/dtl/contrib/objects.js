define([
	"dojo/_base/lang",
	"../_base"	
], function(lang,dd){

	var objects = lang.getObject("contrib.objects", true, dd);
/*=====
	objects = {
		// TODO: summary
	};
=====*/

	lang.mixin(objects, {
		key: function(value, arg){
			return value[arg];
		}
	});

	dd.register.filters("dojox.dtl.contrib", {
		"objects": ["key"]
	});

	return objects;
});