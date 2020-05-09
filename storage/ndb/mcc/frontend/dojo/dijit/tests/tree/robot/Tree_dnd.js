/*
 * Helper functions for Tree_dnd.html and Tree_dnd_multiParent.html tests
 */

define([
	"doh/runner", "dojo/robotx",
	"dojo/_base/array", "dojo/dnd/autoscroll", "dojo/dom", "dojo/dom-geometry", "dojo/query", "dojo/_base/window",
	"dijit/tests/helpers"
], function(doh, robot, array, autoscroll, dom, domGeom, query, win, helpers){

	var exports = {
		setup: function setup(){
			doh.register("setup screen", function(){
				// Hide boilerplate text so it's easier to drag on small screen
				query("h1,h2,p", robot.doc).style("display", "none");

				// Disable auto-scrolling because otherwise the viewport scrolls as robot.mouseMoveAt()
				// moves the mouse, literally making the the drop target a moving target
				// (and mouseMoveAt() doesn't take this possibility into account).
				autoscroll.autoScrollNodes = function(){};

				// Scroll viewport to (try to) make sure that both tree and drag-source
				// are simultaneously in view.
				var scroll = domGeom.position(dom.byId("1001", robot.doc)).y;
				win.body(robot.doc).parentNode.scrollTop = scroll;	// works on FF
				win.body(robot.doc).scrollTop = scroll;	// works on safari
			});

			// Wait for trees to load
			doh.register("wait for load",  {
					name: "wait for load",
					timeout: 10000,
					runTest: helpers.waitForLoad
			});
			doh.register("setup vars", function(){
				registry = robot.window.require("dijit/registry");
			});
		},

		findTreeNode: function findTreeNode(/*String*/ treeId, /*String*/ label){
			// summary:
			//		Find the TreeNode with the specified label in the given tree.
			//		Assumes that there's only one TreeNode w/that label (i.e. it
			//		breaks if certain items have multiple parents and appear in the
			//		tree multiple times)
			var nodes = query(".dijitTreeLabel", dom.byId(treeId, robot.doc));
			for(var i=0; i<nodes.length; i++){
				if(helpers.innerText(nodes[i]) == label){
					return registry.getEnclosingWidget(nodes[i]);	// TreeNode
				}
			}
			return null;
		},

		findTreeNodeByPath: function findTreeNodeByPath(/*String*/ treeId, /*String[] */ path){
			// summary:
			//		Find the TreeNode with the specified path (like ["Fruits", "Apple"] in a tree like:
			//	|	* Foods
			//	|		* Vegetables
			//	|		* Fruits
			//	|			* Orange
			//	|			* Apple
			//		Path shouldn't include the root node.

			var tree = registry.byId(treeId);
			for(var i=0, node=tree.rootNode; i<path.length; i++){
				var pathElem = path[i], matchingChildNode;
				for(var j=0, children=node.getChildren(); j < children.length; j++){
					if(children[j].label == pathElem){
						matchingChildNode = children[j];
						break;
					}
				}
				if(!matchingChildNode){
					return null;
				}
				node = matchingChildNode;
			}
			return node;
		},

		getChildrenOfItem: function getChildrenOfItem(/*String*/ name){
			// summary:
			//		Return the children of the data store item w/the specified name
			//		Note that test_Tree_Dnd.html splits the children up into the "children"
			//		and "items" attributes.

			// Get the parent item
			// Note that the ItemFileWriteStore's callback will happen immediately.
			var myStore = robot.window.myStore,
				parentItem;
			myStore.fetch({
				query: {name: name},
				onItem: function(item){ parentItem = item; }
			});

			// Get the children, which are stored in two separate attributes,
			// categories (like 'Fruits') and items (ie, leaf nodes)  (like 'Apple')
			return {
				categories: myStore.getValues(parentItem, 'children'),
				items: myStore.getValues(parentItem, 'items')
			};
		},

		mapItemsToNames: function mapItemsToNames(ary){
			// summary:
			//		Convert an array of items into an array of names
			var myStore = robot.window.myStore;
			return array.map(ary, function(item){
				return myStore.getValue(item, "name");
			});
		},

		getNamesOfChildrenOfItem: function getNamesOfChildrenOfItem(/*String*/ name){
			// summary:
			//		Return the names of the (items that are) children of the item w/the specified name

			// Get the parent item (according to
			var obj = exports.getChildrenOfItem(name);
			return {
				categories: exports.mapItemsToNames(obj.categories),
				items: exports.mapItemsToNames(obj.items)
			};
		}
	};

	return exports;
});


