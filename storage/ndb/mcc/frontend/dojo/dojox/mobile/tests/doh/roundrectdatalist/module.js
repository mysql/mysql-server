define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	doh.registerUrl("dojox.mobile.tests.doh.RoundRectDataList", require.toUrl("./RoundRectDataList.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.RoundRectDataList", require.toUrl("./RoundRectDataList_Programmatic.html"),999999);
	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.RoundRectDataList", require.toUrl("./RoundRectDataListTests.html"),999999);
	}
});



