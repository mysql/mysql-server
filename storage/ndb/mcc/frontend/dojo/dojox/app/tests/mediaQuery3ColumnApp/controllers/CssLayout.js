define(["dojo/_base/declare", "dojo/dom", "dojo/dom-style",
		"dojo/dom-class", "dojo/dom-attr", "dojo/dom-construct", 
		"dojox/app/controllers/LayoutBase"],
function(declare, dom, domStyle, domClass, domAttr, domConstruct, LayoutBase){
	// module:
	//		dojox/app/tests/mediaQuery3ColumnApp/controllers/CssLayout
	// summary:
	//		Will layout an application with a BorderContainer.  
	//		Each view to be shown in a region of the BorderContainer will be wrapped in a StackContainer and a ContentPane.
	//		

	return declare("dojox/app/tests/mediaQuery3ColumnApp/controllers/CssLayout", LayoutBase, {

		constructor: function(app, events){
			// summary:
			//		bind "app-initLayout" and "app-layoutView" events on application instance.
			//
			// app:
			//		dojox/app application instance.
			// events:
			//		{event : handler}
		},

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
			this.app.log("in app/controllers/CssLayout.initLayout event.view.name=[",event.view.name,"] event.view.parent.name=[",event.view.parent.name,"]");


			this.app.log("in app/controllers/CssLayout.initLayout event.view.constraint=",event.view.constraint);
			var constraint = event.view.constraint;  // constraint holds the region for this view, center, top etc.
			
			event.view.parent.domNode.appendChild(event.view.domNode);
			domClass.add(event.view.domNode, constraint);  // set the class to the constraint

			this.inherited(arguments);
		},

		onResize: function(){
			// do nothing on resize
		},

		hideView: function(view){
			domStyle.set(view.domNode, "display", "none !important");
			//domClass.add(view.domNode, "hide");
		},

		showView: function(view){
			domStyle.set(view.domNode, "display", "block");
			//domClass.remove(view.domNode, "hide");			
		}
	});
});
