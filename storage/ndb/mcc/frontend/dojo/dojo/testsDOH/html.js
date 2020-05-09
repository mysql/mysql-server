define(["doh/main", "require"], function(doh, require){
	if(doh.isBrowser){
		doh.register("testsDOH.html", require.toUrl("./html/test_set.html"), 30000);
	}
});
