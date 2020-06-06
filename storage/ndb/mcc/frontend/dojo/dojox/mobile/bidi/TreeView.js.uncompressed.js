define("dojox/mobile/bidi/TreeView", [
	"dojo/_base/declare"
], function(declare){

	// module:
	//		dojox/mobile/bidi/TreeView

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile TreeView widget, using Unicode Control Characters to control text direction.
		// description:
		//		Text direction attribute of the Tree is set to ListItem.
		//		This class should not be used directly.
		//		Mobile TreeView widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_customizeListItem: function(listItemArgs){
			listItemArgs.textDir = this.textDir;
			if(!this.isLeftToRight()){
				listItemArgs.dir = "rtl";
				listItemArgs.transitionDir = -1;
			}
		}

	});
});
