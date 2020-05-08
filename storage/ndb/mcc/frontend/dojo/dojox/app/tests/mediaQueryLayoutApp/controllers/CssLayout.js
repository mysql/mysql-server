define(["dojo/_base/declare", "dojo/dom", "dojo/dom-style",
		"dojo/dom-class", "dojo/dom-attr", "dojo/dom-construct", 
		"dojox/app/controllers/LayoutBase"],
function(declare, dom, domStyle, domClass, domAttr, domConstruct, LayoutBase){
	// module:
	//		dojox/app/tests/mediaQueryLayoutApp/controllers/CssLayout
	// summary:
	//		Will layout an application with a BorderContainer.  
	//		Each view to be shown in a region of the BorderContainer will be wrapped in a StackContainer and a ContentPane.
	//		

	return declare("dojox/app/tests/mediaQueryLayoutApp/controllers/CssLayout", LayoutBase, {

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
			
		//	if(event.view.parent.id == this.app.id){  // If the parent of this view is the app we are working with the BorderContainer
			//	var reg = dom.byId(event.view.parent.id+"-"+constraint);			
			//	if(reg){  // already has a wrapperNode, just add to it.
			//		reg.appendChild(event.view.domNode);
			//	}else{ // need a wrapperNode
			//		var container;
			//		event.view.parent.domNode.appendChild(container = domConstruct.create("div"));
			//		container.appendChild(event.view.domNode);
			//		domAttr.set(container, "id", event.view.parent.id+"-"+constraint);
			//		domClass.add(event.view.domNode, constraint);  // set the class to the constraint
			//	}
		//	}else{ // Not a top level page transition, so not changing a page in the BorderContainer, so handle it like Layout.
				event.view.parent.domNode.appendChild(event.view.domNode);
				domClass.add(event.view.domNode, constraint);  // set the class to the constraint
		//	}

			this.inherited(arguments);
		},

		onResize: function(){
			// do nothing on resize
		},

		hideView: function(view){
			domStyle.set(view.domNode, "display", "none");
			//var sc = registry.byId(view.parent.id+"-"+view.constraint);
			//if(sc){
			//	sc.removedFromBc = true;
			//	sc.removeChild(view.domNode);
			//}
		},

		showView: function(view){
			domStyle.set(view.domNode, "display", "");
			
			//var sc = registry.byId(view.parent.id+"-"+view.constraint);
			//if(sc){
			//	if(sc.removedFromBc){
			//		sc.removedFromBc = false;
			//		registry.byId(this.app.id+"-BC").addChild(sc);
			//		domStyle.set(view.domNode, "display", "");
			//	}
			//	domStyle.set(cp.domNode, "display", "");
			//	sc.selectChild(cp);
			//	sc.resize();
			//}
		}
	});
});
