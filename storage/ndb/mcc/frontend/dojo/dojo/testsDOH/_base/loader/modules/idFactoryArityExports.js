define("dojo/testsDOH/_base/loader/modules/idFactoryArityExports", function(require, exports, module){
	var impliedDep = require("./impliedDep4");
	require("dojo/_base/lang").mixin(exports, {
		module:module,
		id:"idFactoryArityExports",
		impliedDep:impliedDep.id
	});
});