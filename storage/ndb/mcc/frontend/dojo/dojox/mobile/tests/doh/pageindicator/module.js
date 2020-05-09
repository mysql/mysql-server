define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.PageIndicator", require.toUrl("./PageIndicatorTests1.html"),999999);
		doh.registerUrl("dojox.mobile.tests.doh.PageIndicator", require.toUrl("./PageIndicatorTests2.html"),999999);
		doh.registerUrl("dojox.mobile.tests.doh.PageIndicator", require.toUrl("./PageIndicatorTests3.html"),999999);
	}
});



