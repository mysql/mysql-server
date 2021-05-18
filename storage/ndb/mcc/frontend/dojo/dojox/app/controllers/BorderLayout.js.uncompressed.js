define("dojox/app/controllers/BorderLayout", ["dojo/_base/declare", "dojo/dom-attr", "dojo/dom-style", "./LayoutBase","dijit/layout/BorderContainer",
		"dijit/layout/StackContainer", "dijit/layout/ContentPane", "dijit/registry"],
function(declare, domAttr, domStyle, LayoutBase, BorderContainer, StackContainer, ContentPane, registry){
	// module:
	//		dojox/app/controllers/BorderLayout
	// summary:
	//		Will layout an application with a BorderContainer.  
	//		Each view to be shown in a region of the BorderContainer will be wrapped in a StackContainer and a ContentPane.
	//		

	return declare("dojox.app.controllers.BorderLayout", LayoutBase, {

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
			this.app.log("in app/controllers/BorderLayout.initLayout event.view.name=[",event.view.name,"] event.view.parent.name=[",event.view.parent.name,"]");

			var bc;
			if(!this.borderLayoutCreated){ // If the BorderContainer has not been created yet, create it.
				this.borderLayoutCreated = true;
				bc = new BorderContainer({id:this.app.id+"-BC", style: "height:100%;width:100%;border:1px solid black"});
				event.view.parent.domNode.appendChild(bc.domNode);	// put the border container into the parent (app)

				bc.startup();	// startup the BorderContainer
			}else{
				bc = registry.byId(this.app.id+"-BC");				
			}

			this.app.log("in app/controllers/BorderLayout.initLayout event.view.constraint=",event.view.constraint);
			var constraint = event.view.constraint;	// constraint holds the region for this view, center, top etc.
			
			if(event.view.parent.id == this.app.id){	// If the parent of this view is the app we are working with the BorderContainer
				var reg = registry.byId(event.view.parent.id+"-"+constraint);			
				if(reg){	// already has a stackContainer, just create the contentPane for this view and add it to the stackContainer.
					var cp1 = registry.byId(event.view.id+"-cp-"+constraint);
					if(!cp1){
						cp1 = new ContentPane({id:event.view.id+"-cp-"+constraint});
						cp1.addChild(event.view); // important to add the widget to the cp before adding cp to BorderContainer for height
						reg.addChild(cp1);
						bc.addChild(reg);
					}else{
						cp1.domNode.appendChild(event.view.domNode);
					}
				}else{ // need a contentPane
					// this is where the region (constraint) is set for the BorderContainer's StackContainer
					var noSplitter = this.app.borderLayoutNoSplitter || false;
					var sc1 = new StackContainer({doLayout: true, splitter:!noSplitter, region:constraint, id:event.view.parent.id+"-"+constraint});
					var cp1 = new ContentPane({id:event.view.id+"-cp-"+constraint});
					cp1.addChild(event.view); // should we use addChild or appendChild?
					sc1.addChild(cp1);
					bc.addChild(sc1);
				}
			}else{ // Not a top level page transition, so not changing a page in the BorderContainer, so handle it like Layout.
				event.view.parent.domNode.appendChild(event.view.domNode);
				domAttr.set(event.view.domNode, "data-app-constraint", event.view.constraint);
			}

			this.inherited(arguments);
		},

		hideView: function(view){
			var bc = registry.byId(this.app.id+"-BC");
			var sc = registry.byId(view.parent.id+"-"+view.constraint);
			if(bc && sc){
				sc.removedFromBc = true;
				bc.removeChild(sc);
			}
		},

		showView: function(view){
			var sc = registry.byId(view.parent.id+"-"+view.constraint);
			var cp = registry.byId(view.id+"-cp-"+view.constraint);
			if(sc && cp){
				if(sc.removedFromBc){
					sc.removedFromBc = false;
					registry.byId(this.app.id+"-BC").addChild(sc);
					domStyle.set(view.domNode, "display", "");
				}
				domStyle.set(cp.domNode, "display", "");
				sc.selectChild(cp);
				sc.resize();
			}
		}
	});
});
