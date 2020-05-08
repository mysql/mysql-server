define(["dojo/_base/lang", "dojo/_base/declare", "dojo/sniff", "dojo/_base/window", "dojo/_base/config",
		"dojo/dom-attr", "dojo/topic", "dojo/dom-style", "../utils/constraints", "../Controller"],
function(lang, declare, has, win, config, domAttr, topic, domStyle, constraints, Controller){
	// module:
	//		dojox/app/controllers/LayoutBase
	// summary:
	//		Bind "app-initLayout", "app-layoutView" and "app-resize" events on application instance.

	return declare("dojox.app.controllers.LayoutBase", Controller, {

		constructor: function(app, events){
			// summary:
			//		bind "app-initLayout", "app-layoutView" and "app-resize" events on application instance.
			//
			// app:
			//		dojox/app application instance.
			// events:
			//		{event : handler}
			this.events = {
				"app-initLayout": this.initLayout,
				"app-layoutView": this.layoutView,
				"app-resize": this.onResize
			};
			// if we are using dojo mobile & we are hiding address bar we need to be bit smarter and listen to
			// dojo mobile events instead
			if(config.mblHideAddressBar){
				topic.subscribe("/dojox/mobile/afterResizeAll", lang.hitch(this, this.onResize));
			}else{
				// bind to browsers orientationchange event for ios otherwise bind to browsers resize
				this.bind(win.global, has("ios") ? "orientationchange" : "resize", lang.hitch(this, this.onResize));
			}
		},

		onResize: function(){
			this._doResize(this.app);
			// this is needed to resize the children on an orientation change or a resize of the browser.
			// it was being done in _doResize, but was not needed for every call to _doResize.
			for(var hash in this.app.selectedChildren){	// need this to handle all selectedChildren
				if(this.app.selectedChildren[hash]){
					this._doResize(this.app.selectedChildren[hash]);
				}
			}
			
		},
		
		initLayout: function(event){
			// summary:
			//		Response to dojox/app "app-initLayout" event.
			//
			// example:
			//		Use emit to trigger "app-initLayout" event, and this function will respond to the event. For example:
			//		|	this.app.emit("app-initLayout", view);
			//
			// event: Object
			// |		{"view": view, "callback": function(){}};
			domAttr.set(event.view.domNode, "id", event.view.id);	// Set the id for the domNode
			if(event.callback){	// if the event has a callback, call it.
				event.callback();
			}
		},

		_doLayout: function(view){
			// summary:
			//		do view layout.
			//
			// view: Object
			//		view instance needs to do layout.

			if(!view){
				console.warn("layout empty view.");
			}
		},

		_doResize: function(view){
			// summary:
			//		resize view.
			//
			// view: Object
			//		view instance needs to do resize.
			this.app.log("in LayoutBase _doResize called for view.id="+view.id+" view=",view);
			this._doLayout(view);
		},

		layoutView: function(event){
			// summary:
			//		Response to dojox/app "app-layoutView" event.
			//
			// example:
			//		Use emit to trigger "app-layoutView" event, and this function will response the event. For example:
			//		|	this.app.emit("app-layoutView", view);
			//
			// event: Object
			// |		{"parent":parent, "view":view, "removeView": boolean}
			var parent = event.parent || this.app;
			var view = event.view;

			if(!view){
				return;
			}

			this.app.log("in LayoutBase layoutView called for event.view.id="+event.view.id);

			// if the parent has a child in the view constraint it has to be hidden, and this view displayed.
			var parentSelChild = constraints.getSelectedChild(parent, view.constraint);
			if(event.removeView){	// if this view is being removed set display to none and the selectedChildren entry to null
				view.viewShowing = false;
				this.hideView(view);
				if(view == parentSelChild){
					constraints.setSelectedChild(parent, view.constraint, null);	// remove from selectedChildren
				}
			}else if(view !== parentSelChild){
				if(parentSelChild){
				//	domStyle.set(parentSelChild.domNode, "zIndex", 25);
					parentSelChild.viewShowing = false;
					if(event.transition == "none" || event.currentLastSubChildMatch !== parentSelChild){
						this.hideView(parentSelChild); // only call hideView for transition none or when the transition will not hide it
					}
				}
				view.viewShowing = true;
				this.showView(view);
				//domStyle.set(view.domNode, "zIndex", 50);
				constraints.setSelectedChild(parent, view.constraint, view);
			}else{ // this view is already the selected child and showing
				view.viewShowing = true;
			}
		},

		hideView: function(view){
			this.app.log("logTransitions:","LayoutBase"+" setting domStyle display none for view.id=["+view.id+"], visibility=["+view.domNode.style.visibility+"]");
			domStyle.set(view.domNode, "display", "none");
		},

		showView: function(view){
			if(view.domNode){
				this.app.log("logTransitions:","LayoutBase"+" setting domStyle display to display for view.id=["+view.id+"], visibility=["+view.domNode.style.visibility+"]");
				domStyle.set(view.domNode, "display", "");
			}
		}
	});
});
