define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	if(!(has("ie") < 10)){
		doh.registerUrl("dojox.mobile.tests.doh.ContentPane", require.toUrl("./ContentPaneTests.html"),999999);
	}
});



