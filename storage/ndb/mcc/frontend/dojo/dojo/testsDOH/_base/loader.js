define([
	"dojo",
	"doh",
	"require",
	"dojo/sniff",
	"./loader/bootstrap"], function(dojo, doh, require, has){
	if(doh.isBrowser){

		//TODO: doh.register("testsDOH._base.loader.cdn-load", require.toUrl("./loader/cdnTest.html"));

		doh.register("testsDOH._base.loader.top-level-module-by-paths", require.toUrl("./loader/paths.html"));
	}
});

