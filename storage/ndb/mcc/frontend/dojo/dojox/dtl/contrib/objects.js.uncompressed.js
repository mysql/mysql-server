//>>built
define("dojox/dtl/contrib/objects", [
	"dojo/_base/lang",
	"../_base"	
], function(lang,dd){
	/*=====
		dd = dojox.dtl;
	=====*/
	lang.getObject("dojox.dtl.contrib.objects", true);

	lang.mixin(dd.contrib.objects, {
		key: function(value, arg){
			return value[arg];
		}
	});

	dd.register.filters("dojox.dtl.contrib", {
		"objects": ["key"]
	});
	return dojox.dtl.contrib.objects;
});