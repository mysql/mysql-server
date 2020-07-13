define("dojox/app/controllers/Layout", ["dojo/_base/declare", "dojo/_base/lang", "dojo/_base/array", "dojo/_base/window",
		"dojo/query", "dojo/dom-geometry", "dojo/dom-attr", "dojo/dom-style", "dijit/registry",
		"./LayoutBase", "../utils/layout", "../utils/constraints", "dojo/sniff"],
function(declare, lang, array, win, query, domGeom, domAttr, domStyle, registry, LayoutBase, layout, constraints, has){
	// module:
	//		dojox/app/controllers/Layout
	// summary:
	//		Extends LayoutBase which binds "app-initLayout", "app-layoutView" and "app-resize" events on application instance.

	return declare("dojox.app.controllers.Layout", LayoutBase, {

		constructor: function(app, events){
			// summary:
			//		bind "app-initLayout" and "app-layoutView" events on application instance.
			//
			// app:
			//		dojox/app application instance.
			// events:
			//		{event : handler}
		},

		onResize: function(){
			this._doResize(this.app);
			// this is needed to resize the children on an orientation change or a resize of the browser.
			// it was being done in _doResize, but was not needed for every call to _doResize.
			this.resizeSelectedChildren(this.app);
		},


		resizeSelectedChildren: function(w){
			for(var hash in w.selectedChildren){	// need this to handle all selectedChildren
				if(w.selectedChildren[hash] && w.selectedChildren[hash].domNode){
					this.app.log("in Layout resizeSelectedChildren calling resizeSelectedChildren calling _doResize for w.selectedChildren[hash].id="+w.selectedChildren[hash].id);
					this._doResize(w.selectedChildren[hash]);
					// Call resize on child widgets, needed to get the scrollableView to resize correctly initially	
					array.forEach(w.selectedChildren[hash].domNode.children, function(child){
						if(registry.byId(child.id) && registry.byId(child.id).resize){ 
							registry.byId(child.id).resize(); 
						}
					});	

					this.resizeSelectedChildren(w.selectedChildren[hash]);
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
			this.app.log("in app/controllers/Layout.initLayout event=",event);
			this.app.log("in app/controllers/Layout.initLayout event.view.parent.name=[",event.view.parent.name,"]");

			if (!event.view.domNode.parentNode || (has("ie") == 8 && !event.view.domNode.parentElement)) {
				if(this.app.useConfigOrder){
					event.view.parent.domNode.appendChild(event.view.domNode);
				}else{
					this.addViewToParentDomByConstraint(event);
				}
			}
			domAttr.set(event.view.domNode, "data-app-constraint", event.view.constraint);
			this.inherited(arguments);
		},

		addViewToParentDomByConstraint: function(event){
			// summary:
			//		Insert the view domNode into the parent domNode based upon the constraints.
			//		It should layout the children in this order: top, left, center, right, bottom
			//		Unless it is rtl then it should layout the children in this order: top, right, center, left, bottom
			//
			// event: Object
			// |		{"parent":parent, "view":view, "removeView": boolean}
			var newViewConstraint = event.view.constraint;
			if(newViewConstraint === "bottom"){ // if new is bottom always place last
				event.view.parent.domNode.appendChild(event.view.domNode);
			}else if(newViewConstraint === "top"){ // if new is top always place first
				event.view.parent.domNode.insertBefore(event.view.domNode, event.view.parent.domNode.firstChild);
			}else{ // need to compare new constraint to the previous ones
				if(event.view.parent.domNode.children.length > 0){ // parent node has children, check constraints
					// in this loop if previous is top or left skip it and look for next child, otherwise process it
					for(var childIndex in event.view.parent.domNode.children){
						var child = event.view.parent.domNode.children[childIndex];
						var dir = domStyle.get(event.view.parent.domNode,"direction");
						var isltr = (dir === "ltr");
						var LEADING_VIEW = isltr ? "left" : "right";
						var TRAILING_VIEW = isltr ? "right" : "left";
						if(child.getAttribute && child.getAttribute("data-app-constraint")) {
							var previousViewConstraint = child.getAttribute("data-app-constraint");
 							// if previous is bottom or previous is Trailing
 							// or previous is not top and newView is Leading we need to insert before this child
							if(previousViewConstraint === "bottom" ||
								(previousViewConstraint === TRAILING_VIEW) ||
								(previousViewConstraint !== "top" &&
									(newViewConstraint === LEADING_VIEW))){
								event.view.parent.domNode.insertBefore(event.view.domNode, child);
								break;
							}
						}
					}
				}
			}
			// if the domNode was not added to the parent yet add it to the end now
			if (!event.view.domNode.parentNode || (has("ie") == 8 && !event.view.domNode.parentElement)) {
				event.view.parent.domNode.appendChild(event.view.domNode);
			}
		},

		_doResize: function(view){
			// summary:
			//		resize view.
			//
			// view: Object
			//		view instance needs to do layout.
			var node = view.domNode;
			if(!node){
				this.app.log("Warning - View has not been loaded, in Layout _doResize view.domNode is not set for view.id="+view.id+" view=",view);
				return;
			}

			// If either height or width wasn't specified by the user, then query node for it.
			// But note that setting the margin box and then immediately querying dimensions may return
			// inaccurate results, so try not to depend on it.
			var mb = {};
			if( !("h" in mb) || !("w" in mb) ){
				mb = lang.mixin(domGeom.getMarginBox(node), mb);	// just use dojo/_base/html.marginBox() to fill in missing values
			}

			// Compute and save the size of my border box and content box
			// (w/out calling dojo/_base/html.contentBox() since that may fail if size was recently set)
			if(view !== this.app){
				var cs = domStyle.getComputedStyle(node);
				var me = domGeom.getMarginExtents(node, cs);
				var be = domGeom.getBorderExtents(node, cs);
				var bb = (view._borderBox = {
					w: mb.w - (me.w + be.w),
					h: mb.h - (me.h + be.h)
				});
				var pe = domGeom.getPadExtents(node, cs);
				view._contentBox = {
					l: domStyle.toPixelValue(node, cs.paddingLeft),
					t: domStyle.toPixelValue(node, cs.paddingTop),
					w: bb.w - pe.w,
					h: bb.h - pe.h
				};
			}else{
				// if we are layouting the top level app the above code does not work when hiding address bar
				// so let's use similar code to dojo mobile.
				view._contentBox = {
					l: 0,
					t: 0,
					h: win.global.innerHeight || win.doc.documentElement.clientHeight,
					w: win.global.innerWidth || win.doc.documentElement.clientWidth
				};
			}

			this.inherited(arguments);
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
			if(event.view){
				this.inherited(arguments);
				// normally when called from transition doResize will be false, and the resize will only be done when the app-resize event is fired
				if(event.doResize){
					this._doResize(event.parent || this.app);
					this._doResize(event.view);
				}
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
				return;
			}
			this.app.log("in Layout _doLayout called for view.id="+view.id+" view=",view);

			var children;
			// TODO: probably need to handle selectedChildren here, not just selected child...
			// TODO: why are we passing view here? not parent? This call does not seem logical?
			var selectedChild = constraints.getSelectedChild(view, view.constraint);
			if(selectedChild && selectedChild.isFullScreen){
				console.warn("fullscreen sceen layout");
				/*
				 fullScreenScene=true;
				 children=[{domNode: selectedChild.domNode,constraint: "center"}];
				 query("> [constraint]",this.domNode).forEach(function(c){
				 if(selectedChild.domNode!==c.domNode){
				 dstyle(c.domNode,"display","none");
				 }
				 })
				 */
			}else{
				children = query("> [data-app-constraint]", view.domNode).map(function(node){
					var w = registry.getEnclosingWidget(node);
					if(w){
						w._constraint = domAttr.get(node, "data-app-constraint");
						return w;
					}

					return {
						domNode: node,
						_constraint: domAttr.get(node, "data-app-constraint")
					};
				});
				
				if(selectedChild){
					children = array.filter(children, function(c){
						// do not need to set display none here it is set in select.
						return c.domNode && c._constraint;
					}, view);
				}
			}
			// We don't need to layout children if this._contentBox is null for the operation will do nothing.
			if(view._contentBox){
				layout.layoutChildren(view.domNode, view._contentBox, children);
			}
		}
	});
});
