//>>built
define("dijit/_base/place", [
	"dojo/_base/array", // array.forEach
	"dojo/_base/lang", // lang.isArray
	"dojo/window", // windowUtils.getBox
	"../place",
	".."	// export to dijit namespace
], function(array, lang, windowUtils, place, dijit){

	// module:
	//		dijit/_base/place
	// summary:
	//		Back compatibility module, new code should use dijit/place directly instead of using this module.

	dijit.getViewport = function(){
		// summary:
		//		Deprecated method to return the dimensions and scroll position of the viewable area of a browser window.
		//		New code should use windowUtils.getBox()

		return windowUtils.getBox();
	};

	/*=====
	dijit.placeOnScreen = function(node, pos, corners, padding){
		// summary:
		//		Positions one of the node's corners at specified position
		//		such that node is fully visible in viewport.
		//		Deprecated, new code should use dijit.place.at() instead.
	};
	=====*/
	dijit.placeOnScreen = place.at;

	/*=====
	dijit.placeOnScreenAroundElement = function(node, aroundElement, aroundCorners, layoutNode){
		// summary:
		//		Like dijit.placeOnScreenAroundNode(), except it accepts an arbitrary object
		//		for the "around" argument and finds a proper processor to place a node.
		//		Deprecated, new code should use dijit.place.around() instead.
	};
	====*/
	dijit.placeOnScreenAroundElement = function(node, aroundNode, aroundCorners, layoutNode){
		// Convert old style {"BL": "TL", "BR": "TR"} type argument
		// to style needed by dijit.place code:
		//		[
		// 			{aroundCorner: "BL", corner: "TL" },
		//			{aroundCorner: "BR", corner: "TR" }
		//		]
		var positions;
		if(lang.isArray(aroundCorners)){
			positions = aroundCorners;
		}else{
			positions = [];
			for(var key in aroundCorners){
				positions.push({aroundCorner: key, corner: aroundCorners[key]});
			}
		}

		return place.around(node, aroundNode, positions, true, layoutNode);
	};

	/*=====
	dijit.placeOnScreenAroundNode = function(node, aroundNode, aroundCorners, layoutNode){
		// summary:
		//		Position node adjacent or kitty-corner to aroundNode
		//		such that it's fully visible in viewport.
		//		Deprecated, new code should use dijit.place.around() instead.
	};
	=====*/
	dijit.placeOnScreenAroundNode = dijit.placeOnScreenAroundElement;

	/*=====
	dijit.placeOnScreenAroundRectangle = function(node, aroundRect, aroundCorners, layoutNode){
		// summary:
		//		Like dijit.placeOnScreenAroundNode(), except that the "around"
		//		parameter is an arbitrary rectangle on the screen (x, y, width, height)
		//		instead of a dom node.
		//		Deprecated, new code should use dijit.place.around() instead.
	};
	=====*/
	dijit.placeOnScreenAroundRectangle = dijit.placeOnScreenAroundElement;

	dijit.getPopupAroundAlignment = function(/*Array*/ position, /*Boolean*/ leftToRight){
		// summary:
		//		Deprecated method, unneeded when using dijit/place directly.
		//		Transforms the passed array of preferred positions into a format suitable for
		//		passing as the aroundCorners argument to dijit.placeOnScreenAroundElement.
		//
		// position: String[]
		//		This variable controls the position of the drop down.
		//		It's an array of strings with the following values:
		//
		//			* before: places drop down to the left of the target node/widget, or to the right in
		//			  the case of RTL scripts like Hebrew and Arabic
		//			* after: places drop down to the right of the target node/widget, or to the left in
		//			  the case of RTL scripts like Hebrew and Arabic
		//			* above: drop down goes above target node
		//			* below: drop down goes below target node
		//
		//		The list is positions is tried, in order, until a position is found where the drop down fits
		//		within the viewport.
		//
		// leftToRight: Boolean
		//		Whether the popup will be displaying in leftToRight mode.
		//
		var align = {};
		array.forEach(position, function(pos){
			var ltr = leftToRight;
			switch(pos){
				case "after":
					align[leftToRight ? "BR" : "BL"] = leftToRight ? "BL" : "BR";
					break;
				case "before":
					align[leftToRight ? "BL" : "BR"] = leftToRight ? "BR" : "BL";
					break;
				case "below-alt":
					ltr = !ltr;
					// fall through
				case "below":
					// first try to align left borders, next try to align right borders (or reverse for RTL mode)
					align[ltr ? "BL" : "BR"] = ltr ? "TL" : "TR";
					align[ltr ? "BR" : "BL"] = ltr ? "TR" : "TL";
					break;
				case "above-alt":
					ltr = !ltr;
					// fall through
				case "above":
				default:
					// first try to align left borders, next try to align right borders (or reverse for RTL mode)
					align[ltr ? "TL" : "TR"] = ltr ? "BL" : "BR";
					align[ltr ? "TR" : "TL"] = ltr ? "BR" : "BL";
					break;
			}
		});
		return align;
	};

	return dijit;
});
