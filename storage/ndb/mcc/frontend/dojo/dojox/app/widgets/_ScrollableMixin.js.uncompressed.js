// This module is modified from dojox/mobile/_ScrollableMixin and dojox/mobile/ScrollableView
define("dojox/app/widgets/_ScrollableMixin", [
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/_base/array",
	"dojo/_base/window",
	"dojo/dom-class",
	"dijit/registry",
	"dojo/dom",
	"dojo/dom-construct",
	"dojox/mobile/scrollable"],
	function(declare, lang, array, win, domClass, registry, dom, domConstruct, Scrollable){
	// module:
	//		dojox/mobile/_ScrollableMixin
	// summary:
	//		Mixin for widgets to have a touch scrolling capability.

	return declare("dojox.app.widgets._ScrollableMixin", Scrollable, {
		// summary:
		//		Mixin for widgets to have a touch scrolling capability.
		// description:
		//		Actual implementation is in dojox/mobile/scrollable.js.
		//		scrollable.js is not a dojo class, but just a collection
		//		of functions. This module makes scrollable.js a dojo class.

		// scrollableParams: Object
		//		Parameters for dojox/mobile/scrollable.init().
		scrollableParams: null,

		// appBars: Boolean
		//		Enables the search for application-specific bars (header or footer).
		appBars: true, 

		// allowNestedScrolls: Boolean
		//		e.g. Allow ScrollableView in a SwapView.
		allowNestedScrolls: true,

		constructor: function(){
			this.scrollableParams = {noResize: true}; // set noResize to true to match the way it was done in app/widgets/scrollable.js
		},

		destroy: function(){
			this.cleanup();
			this.inherited(arguments);
		},

		startup: function(){
			if(this._started){ return; }
			this.findAppBars();
			var node, params = this.scrollableParams;
			if(this.fixedHeader){
				node = dom.byId(this.fixedHeader);
				if(node.parentNode == this.domNode){ // local footer
					this.isLocalHeader = true;
				}
				params.fixedHeaderHeight = node.offsetHeight;
			}
			if(this.fixedFooter){
				node = dom.byId(this.fixedFooter);
				if(node){
					if(node.parentNode == this.domNode){ // local footer
						this.isLocalFooter = true;
						node.style.bottom = "0px";
					}
					params.fixedFooterHeight = node.offsetHeight;
				}
			}

			this.init(params);
			this.inherited(arguments);
			this.reparent();
		},

		// build scrollable container domNode. This method from dojox/mobile/ScrollableView
		buildRendering: function(){
			this.inherited(arguments);
			domClass.add(this.domNode, "mblScrollableView");
			this.domNode.style.overflow = "hidden";
			this.domNode.style.top = "0px";
			this.containerNode = domConstruct.create("div",	{className:"mblScrollableViewContainer"}, this.domNode);
			this.containerNode.style.position = "absolute";
			this.containerNode.style.top = "0px"; // view bar is relative
			if(this.scrollDir === "v"){
				this.containerNode.style.width = "100%";
			}
		},

		// This method from dojox/mobile/ScrollableView
		reparent: function(){
			// summary:
			//		Moves all the children to containerNode.
			var i, idx, len, c;
			for(i = 0, idx = 0, len = this.domNode.childNodes.length; i < len; i++){
				c = this.domNode.childNodes[idx];
				// search for view-specific header or footer
		//		if(c === this.containerNode){
				if(c === this.containerNode || this.checkFixedBar(c, true)){
					idx++;
					continue;
				}
				this.containerNode.appendChild(this.domNode.removeChild(c));
			}
		},

		// This method from dojox/mobile/ScrollableView
		resize: function(){
			// summary:
			//		Calls resize() of each child widget.
			this.inherited(arguments); // scrollable#resize() will be called
			array.forEach(this.getChildren(), function(child){
				if(child.resize){ child.resize(); }
			});
		},


		findAppBars: function(){
			// summary:
			//		Search for application-specific header or footer.
			if(!this.appBars){ return; }
			var i, len, c;
			for(i = 0, len = win.body().childNodes.length; i < len; i++){
				c = win.body().childNodes[i];
				this.checkFixedBar(c, false);
			}
			if(this.domNode.parentNode){
				for(i = 0, len = this.domNode.parentNode.childNodes.length; i < len; i++){
					c = this.domNode.parentNode.childNodes[i];
					this.checkFixedBar(c, false);
				}
			}
			this.fixedFooterHeight = this.fixedFooter ? this.fixedFooter.offsetHeight : 0;
		},

	
		checkFixedBar: function(/*DomNode*/node, /*Boolean*/local){
			// summary:
			//		Checks if the given node is a fixed bar or not.
			if(node.nodeType === 1){
				var fixed = node.getAttribute("data-app-constraint")
					|| (registry.byNode(node) && registry.byNode(node)["data-app-constraint"]);
			/*	if(fixed === "top"){
					domClass.add(node, "mblFixedHeaderBar");
					if(local){
						node.style.top = "0px";
						this.fixedHeader = node;
					}
					return fixed;
				}else
			*/	 
				if(fixed === "bottom"){
					domClass.add(node, "mblFixedBottomBar");
					if(local){
						this.fixedFooter = node;
					}else{
						this._fixedAppFooter = node;
					}
					return fixed;
				}
			}
			return null;
		}
			
	});
});
