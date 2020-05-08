define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	doh.registerUrl("dojox.mobile.tests.doh.EdgeToEdgeDataList", require.toUrl("./EdgeToEdgeDataList.html"),999999);
	doh.registerUrl("dojox.mobile.tests.doh.EdgeToEdgeDataList", require.toUrl("./EdgeToEdgeDataList_Programmatic.html"),999999);
	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.EdgeToEdgeDataList", require.toUrl("./EdgeToEdgeDataListTests.html"),999999);
	}
});



