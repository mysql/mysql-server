define("dojox/jsonPath", ["dojo/_base/kernel", 
	"./jsonPath/query"
],function(kernel, query){
	/*=====
	 return {
	 // summary:
	 //		Deprecated.  Should require dojox/jsonPath modules directly rather than trying to access them through
	 //		this module.
	 };
	 =====*/
	kernel.deprecated("dojox/jsonPath: The dojox/jsonPath root module is deprecated, use dojox/jsonPath/query", "", "2.0");
	return {query: query};
});
