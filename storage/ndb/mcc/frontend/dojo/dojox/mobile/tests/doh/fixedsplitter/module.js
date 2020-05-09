define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.FixedSplitter", require.toUrl("./FixedSplitterTests1.html"),999999);
		doh.registerUrl("dojox.mobile.tests.doh.FixedSplitter", require.toUrl("./FixedSplitterTests2.html"),999999);
		doh.registerUrl("dojox.mobile.tests.doh.FixedSplitter", require.toUrl("./FixedSplitterTests3.html"),999999);
	}
});



