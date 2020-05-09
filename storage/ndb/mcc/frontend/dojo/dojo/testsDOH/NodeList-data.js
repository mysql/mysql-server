define(["doh/main", "require"], function(doh, require){
	if(doh.isBrowser){
		doh.register("testsDOH.NodeList-data", require.toUrl("./NodeList-data.html"), 30000);
	}
});
