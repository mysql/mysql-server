define(["dojo/_base/declare", "dojo/dom", "dojo/dom-style",
		"dojo/dom-class", "dojo/dom-attr", "dojo/dom-construct", 
		"dojox/app/controllers/LayoutBase"],
function(declare, dom, domStyle, domClass, domAttr, domConstruct, LayoutBase){
	// module:
	//		dojox/app/tests/swapViewTestApp/controllers/DivIdBasedLayout
	// summary:
	//		Will layout an application based upon div ids.  
	//		Each view will be appended inside the div with the id that matches the value set in the constraints for the view.
	//		

	return declare("dojox/app/tests/swapViewTestApp/controllers/DivIdBasedLayout", LayoutBase, {

		initLayout: function(event){
			// summary:
			//		Response to dojox/app "app-initLayout" event which is setup in LayoutBase.
			//		The initLayout event is called once when the View is being created the first time.
			//
			// example:
			//		Use emit to trigger "app-initLayout" event, and this function will respond to the event. For example:
			//		|	this.app.emit("app-initLayout", view);
			//
			// event: Object
			// |		{"view": view, "callback": function(){}};
			this.app.log("in app/controllers/DivIdBasedLayout.initLayout event.view.name=[",event.view.name,"] event.view.parent.name=[",event.view.parent.name,"]");


			this.app.log("in app/controllers/DivIdBasedLayout.initLayout event.view.constraint=",event.view.constraint);
			var constraint = event.view.constraint;  // constraint holds the region for this view, center, top etc.
			var parentDiv = dom.byId(constraint);
			if(parentDiv){  // If the parentDiv is found append this views domNode to it
				parentDiv.appendChild(event.view.domNode);
			}else{
				event.view.parent.domNode.appendChild(event.view.domNode);
			}
			domClass.add(event.view.domNode, constraint);  // set the class to the constraint

			this.inherited(arguments);
		},

		onResize: function(){
			// do nothing on resize
		},

		hideView: function(view){
			domStyle.set(view.domNode, "display", "none");
		},

		showView: function(view){
			domStyle.set(view.domNode, "display", "");
		}
	});
});
