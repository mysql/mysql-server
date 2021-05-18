define("dojox/mobile/EdgeToEdgeDataList", [
	"dojo/_base/kernel",
	"dojo/_base/declare",
	"./EdgeToEdgeList",
	"./_DataListMixin"
], function(kernel, declare, EdgeToEdgeList, DataListMixin){

	// module:
	//		dojox/mobile/EdgeToEdgeDataList

	kernel.deprecated("dojox/mobile/EdgeToEdgeDataList", 
		"Use dojox/mobile/EdgeToEdgeStoreList instead", "2.0");
	
	return declare("dojox.mobile.EdgeToEdgeDataList", [EdgeToEdgeList, DataListMixin],{
		// summary:
		//		A dojo/data-enabled version of EdgeToEdgeList.
		// description:
		//		EdgeToEdgeDataList is a subclass of EdgeToEdgeList which
		//		can generate ListItems according to the given dojo/data store.
	});
});
