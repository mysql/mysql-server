define("dojox/mobile/SwapView", [
	"dojo/_base/array",
	"dojo/_base/connect",
	"dojo/_base/declare",
	"dojo/dom",
	"dojo/dom-class",
	"dijit/registry",
	"./View",
	"./_ScrollableMixin",
	"./sniff",
	"./_css3",
	"dojo/has!dojo-bidi?dojox/mobile/bidi/SwapView"
], function(array, connect, declare, dom, domClass, registry, View, ScrollableMixin, has, css3, BidiSwapView){

	// module:
	//		dojox/mobile/SwapView

	var SwapView = declare(has("dojo-bidi") ? "dojox.mobile.NonBidiSwapView" : "dojox.mobile.SwapView", [View, ScrollableMixin], {
		// summary:
		//		A container that can be swiped horizontally.
		// description:
		//		SwapView is a container widget which can be swiped horizontally. 
		//		SwapView is a subclass of dojox/mobile/View. It allows the user to 
		//		swipe the screen left or right to move between the views. When 
		//		SwapView is swiped, it finds an adjacent SwapView to open. When 
		//		the transition is done, a topic "/dojox/mobile/viewChanged" is 
		//		published. Note that, to behave properly, the SwapView needs to 
		//		occupy the entire width of the screen.

		/* internal properties */	
		// scrollDir: [private] String
		//		Scroll direction, used by dojox/mobile/scrollable (always "f" for this class).
		scrollDir: "f",
		// weight: [private] Number
		//		Frictional weight used to compute scrolling speed.
		weight: 1.2,

		// _endOfTransitionTimeoutHandle: [private] Object
		//		The handle (returned by _WidgetBase.defer) for the timeout set on touchEnd in case
		//      the end of transition event is not fired by the browser.
		_endOfTransitionTimeoutHandle: null,

		buildRendering: function(){
			this.inherited(arguments);
			domClass.add(this.domNode, "mblSwapView");
			this.setSelectable(this.domNode, false);
			this.containerNode = this.domNode;
			this.subscribe("/dojox/mobile/nextPage", "handleNextPage");
			this.subscribe("/dojox/mobile/prevPage", "handlePrevPage");
			this.noResize = true; // not to call resize() from scrollable#init
		},

		startup: function(){
			if(this._started){ return; }
			this.inherited(arguments);
		},

		resize: function(){
			// summary:
			//		Calls resize() of each child widget.
			this.inherited(arguments); // scrollable#resize() will be called
			array.forEach(this.getChildren(), function(child){
				if(child.resize){ child.resize(); }
			});
		},

		onTouchStart: function(/*Event*/e){
			// summary:
			//		Internal function to handle touchStart events.
			if(this._siblingViewsInMotion()){  // Ignore touchstart if the views are already in motion
				this.propagatable ? e.preventDefault() : event.stop(e);
				return;
			}
			var fromTop = this.domNode.offsetTop;
			var nextView = this.nextView(this.domNode);
			if(nextView){
				nextView.stopAnimation();
				domClass.add(nextView.domNode, "mblIn");
				// Temporarily add padding to align with the fromNode while transition
				nextView.containerNode.style.paddingTop = fromTop + "px";
			}
			var prevView = this.previousView(this.domNode);
			if(prevView){
				prevView.stopAnimation();
				domClass.add(prevView.domNode, "mblIn");
				// Temporarily add padding to align with the fromNode while transition
				prevView.containerNode.style.paddingTop = fromTop + "px";
			}
			this._setSiblingViewsInMotion(true);
			this.inherited(arguments);
		},

		onTouchEnd: function(/*Event*/e){
			if(e){
				if(!this._moved){ // No transition / animation following touchend in this case
					this._setSiblingViewsInMotion(false);
				}else{ // There might be a transition / animation following touchend
					// As the webkitTransitionEndEvent is not always fired, make sure we call this._setSiblingViewsInMotion(false) even
					// if the event is not fired (and onFlickAnimationEnd is not called as a result)
					this._endOfTransitionTimeoutHandle = this.defer(function(){
						this._setSiblingViewsInMotion(false);
					}, 1000);
				}
			}
			this.inherited(arguments);
		},

		handleNextPage: function(/*Widget*/w){
			// summary:
			//		Called when the "/dojox/mobile/nextPage" topic is published.
			var refNode = w.refId && dom.byId(w.refId) || w.domNode;
			if(this.domNode.parentNode !== refNode.parentNode){ return; }
			if(this.getShowingView() !== this){ return; }
			this.goTo(1);
		},

		handlePrevPage: function(/*Widget*/w){
			// summary:
			//		Called when the "/dojox/mobile/prevPage" topic is published.
			var refNode = w.refId && dom.byId(w.refId) || w.domNode;
			if(this.domNode.parentNode !== refNode.parentNode){ return; }
			if(this.getShowingView() !== this){ return; }
			this.goTo(-1);
		},

		goTo: function(/*Number*/dir, /*String?*/moveTo){
			// summary:
			//		Moves to the next or previous view.
			var view = moveTo ? registry.byId(moveTo) :
				((dir == 1) ? this.nextView(this.domNode) : this.previousView(this.domNode));
			if(view && view !== this){
				this.stopAnimation(); // clean-up animation states
				view.stopAnimation();
				this.domNode._isShowing = false; // update isShowing flag
				view.domNode._isShowing = true;
				this.performTransition(view.id, dir, "slide", null, function(){
					connect.publish("/dojox/mobile/viewChanged", [view]);
				});
			}
		},

		isSwapView: function(/*DomNode*/node){
			// summary:
			//		Returns true if the given node is a SwapView widget.
			return (node && node.nodeType === 1 && domClass.contains(node, "mblSwapView"));
		},

		nextView: function(/*DomNode*/node){
			// summary:
			//		Returns the next view.
			for(var n = node.nextSibling; n; n = n.nextSibling){
				if(this.isSwapView(n)){ return registry.byNode(n); }
			}
			return null;
		},

		previousView: function(/*DomNode*/node){
			// summary:
			//		Returns the previous view.
			for(var n = node.previousSibling; n; n = n.previousSibling){
				if(this.isSwapView(n)){ return registry.byNode(n); }
			}
			return null;
		},

		scrollTo: function(/*Object*/to){
			// summary:
			//		Overrides dojox/mobile/scrollable.scrollTo().
			if(!this._beingFlipped){
				var newView, x;
				if(to.x){
					if(to.x < 0){
						newView = this.nextView(this.domNode);
						x = to.x + this.domNode.offsetWidth;
					}else{
						newView = this.previousView(this.domNode);
						x = to.x - this.domNode.offsetWidth;
					}
				}
				if(newView){
					if(newView.domNode.style.display === "none"){
						newView.domNode.style.display = "";
						newView.resize();
					}
					newView._beingFlipped = true;
					newView.scrollTo({x:x});
					newView._beingFlipped = false;
				}
			}
			this.inherited(arguments);
		},

		findDisp: function(/*DomNode*/node){
			// summary:
			//		Overrides dojox/mobile/scrollable.findDisp().
			// description:
			//		When this function is called from scrollable.js, there are
			//		two visible views, one is the current view, the other is the
			//		next view. This function returns the current view, not the
			//		next view, which has the mblIn class.
			if(!domClass.contains(node, "mblSwapView")){
				return this.inherited(arguments);
			}
			if(!node.parentNode){ return null; }
			var nodes = node.parentNode.childNodes;
			for(var i = 0; i < nodes.length; i++){
				var n = nodes[i];
				if(n.nodeType === 1 && domClass.contains(n, "mblSwapView")
				    && !domClass.contains(n, "mblIn") && n.style.display !== "none"){
					return n;
				}
			}
			return node;
		},

		slideTo: function(/*Object*/to, /*Number*/duration, /*String*/easing, /*Object?*/fake_pos){
			// summary:
			//		Overrides dojox/mobile/scrollable.slideTo().
			if(!this._beingFlipped){
				var w = this.domNode.offsetWidth;
				var pos = fake_pos || this.getPos();
				var newView, newX;
				if(pos.x < 0){ // moving to left
					newView = this.nextView(this.domNode);
					if(pos.x < -w/4){ // slide to next
						if(newView){
							to.x = -w;
							newX = 0;
						}
					}else{ // go back
						if(newView){
							newX = w;
						}
					}
				}else{ // moving to right
					newView = this.previousView(this.domNode);
					if(pos.x > w/4){ // slide to previous
						if(newView){
							to.x = w;
							newX = 0;
						}
					}else{ // go back
						if(newView){
							newX = -w;
						}
					}
				}

				if(newView){
					newView._beingFlipped = true;
					newView.slideTo({x:newX}, duration, easing);
					newView._beingFlipped = false;
					newView.domNode._isShowing = (newView && newX === 0);
				}
				this.domNode._isShowing = !(newView && newX === 0);
			}
			this.inherited(arguments);
		},

		onAnimationEnd: function(/*Event*/e){
			// summary:
			//		Overrides dojox/mobile/View.onAnimationEnd().
			if(e && e.target && domClass.contains(e.target, "mblScrollableScrollTo2")){ return; }
			this.inherited(arguments);
		},

		onFlickAnimationEnd: function(/*Event*/e){
			if(this._endOfTransitionTimeoutHandle){
				this._endOfTransitionTimeoutHandle = this._endOfTransitionTimeoutHandle.remove();
			}
			// summary:
			//		Overrides dojox/mobile/scrollable.onFlickAnimationEnd().
			if(e && e.target && !domClass.contains(e.target, "mblScrollableScrollTo2")){ return; }
			this.inherited(arguments);

			if(this.domNode._isShowing){
				// Hide all the views other than the currently showing one.
				// Otherwise, when the orientation is changed, other views
				// may appear unexpectedly.
				array.forEach(this.domNode.parentNode.childNodes, function(c){
					if(this.isSwapView(c)){
						domClass.remove(c, "mblIn");
						if(!c._isShowing){
							c.style.display = "none";
							c.style[css3.name("transform")] = "";
							c.style.left = "0px"; // top/left mode needs this
							// reset the temporaty padding on the container node
							c.style.paddingTop = "";
						}
					}
				}, this);
				connect.publish("/dojox/mobile/viewChanged", [this]);
				// Reset the temporary padding
				this.containerNode.style.paddingTop = "";
			}else if(!has("css3-animations")){
				this.containerNode.style.left = "0px"; // compat mode needs this
			}
			this._setSiblingViewsInMotion(false);
		},

		_setSiblingViewsInMotion: function(/*Boolean*/inMotion){
			var inMotionAttributeValue = inMotion ? "true" : false;
			var parent = this.domNode.parentNode;
			if(parent){
				parent.setAttribute("data-dojox-mobile-swapview-inmotion", inMotionAttributeValue);
			}
		},

		_siblingViewsInMotion: function(){
			var parent = this.domNode.parentNode;
			if(parent){
				return parent.getAttribute("data-dojox-mobile-swapview-inmotion") == "true";
			}else{
				return false;
			}
		}
	});
	return has("dojo-bidi") ? declare("dojox.mobile.SwapView", [SwapView, BidiSwapView]) : SwapView;
});
