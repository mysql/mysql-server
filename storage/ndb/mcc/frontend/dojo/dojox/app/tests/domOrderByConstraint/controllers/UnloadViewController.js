define(["dojo/_base/lang","dojo/_base/declare", "dojo/Deferred", "dojo/_base/array", "dojo/dom-construct", "dijit/registry", "dojox/app/Controller"],
	function(lang, declare, Deferred, array, domConstruct, registry, Controller){
	// module:
	//		dojox/app/tests/domOrderByConstraint/controllers/UnloadViewController
	// summary:
	//		Used to Unload Views when they are no longer needed
	//		Bind "unload-view" event on dojox/app application instance.
	//		Do transition from one view to another view.

	return declare("dojox/app/tests/domOrderByConstraint/controllers/UnloadViewController", Controller, {

		constructor: function(app){
			this.app = app;
			// summary:
			//		bind "app-transition" event on application instance.
			//
			// app:
			//		dojox/app application instance.
			// events:
			//		{event : handler}
			this.events = {
				"unload-view": this.unloadView
			};
		},

		unloadView: function(event){
			// summary:
			//		Response to dojox/app "unload-view" event.
			// 		If a view has children loaded the view and any children of the child will be unloaded.
			//
			// example:
			//		Use trigger() to trigger "unload-view" event, and this function will response the event. For example:
			//		|	this.trigger("unload-view", {"parent":parent, "view":view, "callback":function(){...}});
			//
			// event: Object
			//		unloadView event parameter. It should be like this: {"parent":parent, "view":view, "callback":function(){...}}

			var parent = event.parent || this.app;
			var view = event.view || "";
			var viewId = view.id;

			if(!parent || !view || !viewId){
				console.warn("unload-view event for view with no parent or with an invalid view with view = ", view);
				return;
			}

			if(parent.selectedChildren[viewId]){
				console.warn("unload-view event for a view which is still in use so it can not be unloaded for view id = " + viewId + "'.");
				return;
			}

			if(!parent.children[viewId]){
				console.warn("unload-view event for a view which was not found in parent.children[viewId] for viewId = " + viewId + "'.");
				return;
			}

			this.unloadChild(parent, view);

			// call Load event callback
			if(event.callback){
				event.callback();
			}
		},

		unloadChild: function(parent, viewToUnload){
			// summary:
			//		Unload the view, and all of its child views recursively.
			// 		Destroy all children, destroy all widgets, destroy the domNode, remove the view from the parent.children,
			// 		then destroy the view.
			//
			// parent: Object
			//		parent of this view.
			// viewToUnload: Object
			//		the view to be unloaded.

			for(var child in viewToUnload.children){
				this.unloadChild(viewToUnload, child);  // unload children then unload the view itself
			}
			if(viewToUnload.domNode){
				// destroy all widgets, then destroy the domNode, then destroy the view.
				var widList = registry.findWidgets(viewToUnload.domNode);
				for(var wid in widList){
					widList[wid].destroyRecursive();
				}
				domConstruct.destroy(viewToUnload.domNode);
			}

			delete parent.children[viewToUnload.id]; // remove it from the parents children
			if(viewToUnload.destroy){
				viewToUnload.destroy(); // call destroy for the view.
			}
		}
	});
});
