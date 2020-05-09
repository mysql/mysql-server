define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.SwapView", require.toUrl("./SwapViewTests1.html"),999999);
		doh.registerUrl("dojox.mobile.tests.doh.SwapView", require.toUrl("./SwapViewTests2.html"),999999);
		doh.registerUrl("dojox.mobile.tests.doh.SwapView", require.toUrl("./SwapViewTests3.html"),999999);
	}
});



