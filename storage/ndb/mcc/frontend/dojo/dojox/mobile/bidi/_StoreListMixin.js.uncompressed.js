define("dojox/mobile/bidi/_StoreListMixin", [
	"dojo/_base/declare"
], function(declare){

	// module:
	//		dojox/mobile/bidi/_StoreListMixin

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile _StoreListMixin and _DataListMixin.
		// description:
		//		Property textDir is set to created ListItem.
		//		This class should not be used directly.
		//		Mobile _StoreListMixin and _DataListMixin load this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		createListItem: function(item){
			var w = this.inherited(arguments);
			w.set("textDir", this.textDir);
			return w;
		}
	});
});
