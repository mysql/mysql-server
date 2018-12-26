//>>built
define("dijit/_base/scroll", [
	"dojo/window", // windowUtils.scrollIntoView
	".."	// export symbol to dijit
], function(windowUtils, dijit){
	// module:
	//		dijit/_base/scroll
	// summary:
	//		Back compatibility module, new code should use windowUtils directly instead of using this module.

	dijit.scrollIntoView = function(/*DomNode*/ node, /*Object?*/ pos){
		// summary:
		//		Scroll the passed node into view, if it is not already.
		//		Deprecated, use `windowUtils.scrollIntoView` instead.

		windowUtils.scrollIntoView(node, pos);
	};
});
