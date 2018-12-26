//>>built
define("dojox/fx/easing", ["dojo/_base/kernel", "dojo/fx/easing"],
  function(kernel,easing){
	kernel.deprecated("dojox.fx.easing","Upgraded to Core, use dojo.fx.easing instead","2.0");
	var fxExt = lang.getObject("dojox.fx",true);
	fxExt.easing = easing;
/*=====
	dojox.fx.easing = {
		// summary:
		//		An Alias to `dojo.fx.easing`. Moved to Core in Dojo 1.2.
	};
=====*/
	return easing;
});
