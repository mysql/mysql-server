define("dojox/mobile/bidi/TabBarButton", [
	"dojo/_base/declare",
	"./common",
	"dojo/dom-class"
], function(declare, common, domClass){

	// module:
	//		mobile/bidi/TabBarButton

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile TabBarButton widget, using Unicode Control Characters to control text direction.
		// description:
		//		This class should not be used directly.
		//		Mobile TabBarButton widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setBadgeAttr: function(/*String*/ text){
			this.inherited(arguments);
			this.badgeObj.setTextDir(this.textDir);
		},
		_setIcon: function(icon, n){
			this.inherited(arguments);
			// dojox.mobile mirroring support
			if(this.iconDivNode && !this.isLeftToRight()){
				domClass.remove(this.iconDivNode, "mblTabBarButtonIconArea");	
				domClass.add(this.iconDivNode, "mblTabBarButtonIconAreaRtl");	
			}
		}
	});
});
