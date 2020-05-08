define([
	"../main",
	"dojo/_base/lang",
	"dojo/dom",
	"dojo/_base/sniff"
], function(dojox, lang, dom, has){

	var dgu = lang.getObject("grid.util", true, dojox);

/*=====
dgu = {
	// summary:
	//		grid utility library
};
=====*/

	dgu.na = '...';
	dgu.rowIndexTag = "gridRowIndex";
	dgu.gridViewTag = "gridView";

	dgu.fire = function(ob, ev, args){
		// Find parent node that scrolls, either vertically or horizontally
		function getScrollParent(node, horizontal){
			if(node == null) {
				return null;
			}

			var dimension = horizontal ? 'Width' : 'Height';
			if(node['scroll' + dimension] > node['client' + dimension]){
				return node;
			}else{
				return getScrollParent(node.parentNode, horizontal);
			}
		}

		// In Webkit browsers focusing an element will scroll this element into view.
		// This may even happen if the element already is in view, but near the edge.
		// This may move the element away from the mouse cursor on the first click
		// of a double click and you end up hitting a different element.
		// Avoid this by storing the scroll position and restoring it after focusing.
		var verticalScrollParent, horizontalScrollParent, scrollTop, scrollLeft, obNode;
		if(has("webkit") && (ev == "focus")){
			obNode = ob.domNode ? ob.domNode : ob;
			verticalScrollParent = getScrollParent(obNode, false);
			if(verticalScrollParent){
				scrollTop = verticalScrollParent.scrollTop;
			}
			horizontalScrollParent = getScrollParent(obNode, true);
			if(horizontalScrollParent){
				scrollLeft = horizontalScrollParent.scrollLeft;
			}
		}

		var fn = ob && ev && ob[ev];
		var result = fn && (args ? fn.apply(ob, args) : ob[ev]());

		// Restore scrolling position
		if(has("webkit") && (ev == "focus")){
			if(verticalScrollParent){
				verticalScrollParent.scrollTop = scrollTop;
			}
			if(horizontalScrollParent){
				horizontalScrollParent.scrollLeft = scrollLeft;
			}
		}

		return result;
	};

	dgu.setStyleHeightPx = function(inElement, inHeight){
		if(inHeight >= 0){
			var s = inElement.style;
			var v = inHeight + 'px';
			if(inElement && s['height'] != v){
				s['height'] = v;
			}
		}
	};

	dgu.mouseEvents = [ 'mouseover', 'mouseout', /*'mousemove',*/ 'mousedown', 'mouseup', 'click', 'dblclick', 'contextmenu' ];

	dgu.keyEvents = [ 'keyup', 'keydown', 'keypress' ];

	dgu.funnelEvents = function(inNode, inObject, inMethod, inEvents){
		var evts = (inEvents ? inEvents : dgu.mouseEvents.concat(dgu.keyEvents));
		for (var i=0, l=evts.length; i<l; i++){
			inObject.connect(inNode, 'on' + evts[i], inMethod);
		}
	};

	dgu.removeNode = function(inNode){
		inNode = dom.byId(inNode);
		inNode && inNode.parentNode && inNode.parentNode.removeChild(inNode);
		return inNode;
	};

	dgu.arrayCompare = function(inA, inB){
		for(var i=0,l=inA.length; i<l; i++){
			if(inA[i] != inB[i]){return false;}
		}
		return (inA.length == inB.length);
	};

	dgu.arrayInsert = function(inArray, inIndex, inValue){
		if(inArray.length <= inIndex){
			inArray[inIndex] = inValue;
		}else{
			inArray.splice(inIndex, 0, inValue);
		}
	};

	dgu.arrayRemove = function(inArray, inIndex){
		inArray.splice(inIndex, 1);
	};

	dgu.arraySwap = function(inArray, inI, inJ){
		var cache = inArray[inI];
		inArray[inI] = inArray[inJ];
		inArray[inJ] = cache;
	};

	return dgu;

});
