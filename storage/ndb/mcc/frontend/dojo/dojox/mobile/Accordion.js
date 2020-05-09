define([
	"dojo/_base/array",
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/sniff",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dojo/dom-attr",
	"dijit/_Contained",
	"dijit/_Container",
	"dijit/_WidgetBase",
	"./iconUtils",
	"./lazyLoadUtils",
	"./_css3",
	"./common",
	"require",
	"dojo/has!dojo-bidi?dojox/mobile/bidi/Accordion"
], function(array, declare, lang, has, domClass, domConstruct, domAttr, Contained, Container, WidgetBase, iconUtils, lazyLoadUtils, css3, common, require, BidiAccordion){

	// module:
	//		dojox/mobile/Accordion

	// inner class
	var _AccordionTitle = declare([WidgetBase, Contained], {
		// summary:
		//		A widget for the title of the accordion.
	
		// label: String
		//		The title of the accordion.
		label: "Label",
		
		// icon1: String
		//		A path for the unselected (typically dark) icon. If icon is not
		//		specified, the iconBase parameter of the parent widget is used.
		icon1: "",

		// icon2: String
		//		A path for the selected (typically highlight) icon. If icon is
		//		not specified, the iconBase parameter of the parent widget or
		//		icon1 is used.
		icon2: "",

		// iconPos1: String
		//		The position of an aggregated unselected (typically dark)
		//		icon. IconPos1 is a comma-separated list of values like
		//		top,left,width,height (ex. "0,0,29,29"). If iconPos1 is not
		//		specified, the iconPos parameter of the parent widget is used.
		iconPos1: "",

		// iconPos2: String
		//		The position of an aggregated selected (typically highlight)
		//		icon. IconPos2 is a comma-separated list of values like
		//		top,left,width,height (ex. "0,0,29,29"). If iconPos2 is not
		//		specified, the iconPos parameter of the parent widget or
		//		iconPos1 is used.
		iconPos2: "",

		// selected: Boolean
		//		If true, the widget is in the selected state.
		selected: false,

		// baseClass: String
		//		The name of the CSS class of this widget.
		baseClass: "mblAccordionTitle",

		buildRendering: function(){
			this.inherited(arguments);

			var a = this.anchorNode = domConstruct.create("a", {
				className: "mblAccordionTitleAnchor",
				role: "presentation"
			}, this.domNode);

			// text box
			this.textBoxNode = domConstruct.create("div", {className:"mblAccordionTitleTextBox"}, a);
			this.labelNode = domConstruct.create("span", {
				className: "mblAccordionTitleLabel",
				innerHTML: this._cv ? this._cv(this.label) : this.label
			}, this.textBoxNode);
			this._isOnLine = this.inheritParams();

			domAttr.set(this.textBoxNode, "role", "tab"); // A11Y
			domAttr.set(this.textBoxNode, "tabindex", "0");
		},

		postCreate: function(){
			this.connect(this.domNode, "onclick", "_onClick");
			common.setSelectable(this.domNode, false);
		},

		inheritParams: function(){
			var parent = this.getParent();
			if(parent){
				if(this.icon1 && parent.iconBase &&
					parent.iconBase.charAt(parent.iconBase.length - 1) === '/'){
					this.icon1 = parent.iconBase + this.icon1;
				}
				if(!this.icon1){ this.icon1 = parent.iconBase; }
				if(!this.iconPos1){ this.iconPos1 = parent.iconPos; }
				if(this.icon2 && parent.iconBase &&
					parent.iconBase.charAt(parent.iconBase.length - 1) === '/'){
					this.icon2 = parent.iconBase + this.icon2;
				}
				if(!this.icon2){ this.icon2 = parent.iconBase || this.icon1; }
				if(!this.iconPos2){ this.iconPos2 = parent.iconPos || this.iconPos1; }
			}
			return !!parent;
		},

		_setIcon: function(icon, n){
			// tags:
			//		private
			if(!this.getParent()){ return; } // icon may be invalid because inheritParams is not called yet
			this._set("icon" + n, icon);
			if(!this["iconParentNode" + n]){
				this["iconParentNode" + n] = domConstruct.create("div",
					{className:"mblAccordionIconParent mblAccordionIconParent" + n}, this.anchorNode, "first");
			}
			this["iconNode" + n] = iconUtils.setIcon(icon, this["iconPos" + n],
				this["iconNode" + n], this.alt, this["iconParentNode" + n]);
			this["icon" + n] = icon;
			domClass.toggle(this.domNode, "mblAccordionHasIcon", icon && icon !== "none");
			if(has("dojo-bidi") && !this.getParent().isLeftToRight()){
				this.getParent()._setIconDir(this["iconParentNode" + n]);
			}
		},

		_setIcon1Attr: function(icon){
			// tags:
			//		private
			this._setIcon(icon, 1);
		},

		_setIcon2Attr: function(icon){
			// tags:
			//		private
			this._setIcon(icon, 2);
		},

		startup: function(){
			if(this._started){ return; }
			if(!this._isOnLine){
				this.inheritParams();
			}
			if(!this._isOnLine){
				this.set({ // retry applying the attribute
					icon1: this.icon1,
					icon2: this.icon2
				});
			}
			this.inherited(arguments);
		},

		_onClick: function(e){
			// summary:
			//		Internal handler for click events.
			// tags:
			//		private
			if(this.onClick(e) === false){ return; } // user's click action
			var p = this.getParent();
			if(!p.fixedHeight && this.contentWidget.domNode.style.display !== "none"){
				p.collapse(this.contentWidget, !p.animation);
			}else{
				p.expand(this.contentWidget, !p.animation);
			}
		},

		onClick: function(/*Event*/ /*===== e =====*/){
			// summary:
			//		User-defined function to handle clicks
			// tags:
			//		callback
		},

		_setSelectedAttr: function(/*Boolean*/selected){
			// tags:
			//		private
			domClass.toggle(this.domNode, "mblAccordionTitleSelected", selected);
			this._set("selected", selected);
		}
	});

	var Accordion = declare(has("dojo-bidi") ? "dojox.mobile.NonBidiAccordion" : "dojox.mobile.Accordion", [WidgetBase, Container, Contained], {
		// summary:
		//		A container widget that can display a group of child panes in a stacked format.
		// description:
		//		Typically, dojox/mobile/Pane, dojox/mobile/Container, or dojox/mobile/ContentPane are 
		//		used as child widgets, but Accordion requires no specific child widget. 
		//		Accordion supports three modes for opening child panes: multiselect, fixed-height,
		//		and single-select. Accordion can have rounded corners, and it can lazy-load the 
		//		content modules.

		// iconBase: String
		//		The default icon path for child widgets.
		iconBase: "",

		// iconPos: String
		//		The default icon position for child widgets.
		iconPos: "",

		// fixedHeight: Boolean
		//		If true, the entire accordion widget has fixed height regardless
		//		of the height of each pane; in this mode, there is always an open pane and
		//		collapsing a pane can only be done by opening a different pane.
		fixedHeight: false,

		// singleOpen: Boolean
		//		If true, only one pane is open at a time. The current open pane
		//		is collapsed, when another pane is opened.
		singleOpen: false,

		// animation: Boolean
		//		If true, animation is used when a pane is opened or
		//		collapsed. The animation works only on webkit browsers.
		animation: true,

		// roundRect: Boolean
		//		If true, the widget shows rounded corners.
		//		Adding the "mblAccordionRoundRect" class to domNode has the same effect.
		roundRect: false,

		/* internal properties */
		duration: .3, // [seconds]

		// baseClass: String
		//		The name of the CSS class of this widget.
		baseClass: "mblAccordion",

		// _openSpace: [private] Number|String 
		_openSpace: 1,

		buildRendering: function(){
			this.inherited(arguments);
			domAttr.set(this.domNode, "role", "tablist"); // A11Y
			domAttr.set(this.domNode, "aria-multiselectable", !this.singleOpen); // A11Y
		},
		
		startup: function(){
			if(this._started){ return; }

			if(domClass.contains(this.domNode, "mblAccordionRoundRect")){
				this.roundRect = true;
			}else if(this.roundRect){
				domClass.add(this.domNode, "mblAccordionRoundRect");
			}

			if(this.fixedHeight){
				this.singleOpen = true;
			}
			var children = this.getChildren();
			array.forEach(children, this._setupChild, this);
			var sel;
			var posinset = 1;
			array.forEach(children, function(child){
				child.startup();
				child._at.startup();
				this.collapse(child, true);
				domAttr.set(child._at.textBoxNode, "aria-setsize", children.length);
				domAttr.set(child._at.textBoxNode, "aria-posinset", posinset++);
				if(child.selected){
					sel = child;
				}
			}, this);
			if(!sel && this.fixedHeight){
				sel = children[children.length - 1];
			}
			if(sel){
				this.expand(sel, true);
			}else{
				this._updateLast();
			}
			this.defer(function(){ this.resize(); });

			this._started = true;
		},

		_setupChild: function(/*Widget*/ child){
			// tags:
			//		private
			if(child.domNode.style.overflow != "hidden"){
				child.domNode.style.overflow = this.fixedHeight ? "auto" : "hidden";
			}
			child._at = new _AccordionTitle({
				label: child.label,
				alt: child.alt,
				icon1: child.icon1,
				icon2: child.icon2,
				iconPos1: child.iconPos1,
				iconPos2: child.iconPos2,
				contentWidget: child
			});
			domConstruct.place(child._at.domNode, child.domNode, "before");
			domClass.add(child.domNode, "mblAccordionPane");
			domAttr.set(child._at.textBoxNode, "aria-controls", child.domNode.id); // A11Y
			domAttr.set(child.domNode, "role", "tabpanel"); // A11Y
			domAttr.set(child.domNode, "aria-labelledby", child._at.id); // A11Y
		},

		addChild: function(/*Widget*/ widget, /*int?*/ insertIndex){
			this.inherited(arguments);
			if(this._started){
				this._setupChild(widget);
				widget._at.startup();
				if(widget.selected){
					this.expand(widget, true);
					this.defer(function(){
						widget.domNode.style.height = "";
					});
				}else{
					this.collapse(widget);
				}
				this._addChildAriaAttrs();
			}
		},

		removeChild: function(/*Widget|int*/ widget){
			if(typeof widget == "number"){
				widget = this.getChildren()[widget];
			}
			if(widget){
				widget._at.destroy();
			}
			this.inherited(arguments);
			this._addChildAriaAttrs();
		},
		
		_addChildAriaAttrs: function(){
			var posinset = 1;
			var children = this.getChildren();
			array.forEach(children, function(child){
				domAttr.set(child._at.textBoxNode, "aria-posinset", posinset++);
				domAttr.set(child._at.textBoxNode, "aria-setsize", children.length);
			});
		},

		getChildren: function(){
			return array.filter(this.inherited(arguments), function(child){
				return !(child instanceof _AccordionTitle);
			});
		},

		getSelectedPanes: function(){
			return array.filter(this.getChildren(), function(pane){
				return pane.domNode.style.display != "none";
			});
		},

		resize: function(){
			if(this.fixedHeight){
				var panes = array.filter(this.getChildren(), function(child){ // active pages
					return child._at.domNode.style.display != "none";
				});
				var openSpace = this.domNode.clientHeight; // height of all panes
				array.forEach(panes, function(child){
					openSpace -= child._at.domNode.offsetHeight;
				});
				this._openSpace = openSpace > 0 ? openSpace : 0;
				var sel = this.getSelectedPanes()[0];
				sel.domNode.style[css3.name("transition")] = "";
				sel.domNode.style.height = this._openSpace + "px";
			}
		},

		_updateLast: function(){
			// tags:
			//		private
			var children = this.getChildren();
			array.forEach(children, function(c, i){
				// add "mblAccordionTitleLast" to the last, closed accordion title
				domClass.toggle(c._at.domNode, "mblAccordionTitleLast",
					i === children.length - 1 && !domClass.contains(c._at.domNode, "mblAccordionTitleSelected"))
			}, this);
		},

		expand: function(/*Widget*/pane, /*boolean*/noAnimation){
			// summary:
			//		Expands the given pane to make it visible.
			// pane:
			//		A pane widget to expand.
			// noAnimation:
			//		If true, the pane expands immediately without animation effect.
			if(pane.lazy){
				lazyLoadUtils.instantiateLazyWidgets(pane.containerNode, pane.requires);
				pane.lazy = false;
			}
			var children = this.getChildren();
			array.forEach(children, function(c, i){
				c.domNode.style[css3.name("transition")] = noAnimation ? "" : "height "+this.duration+"s linear";
				if(c === pane){
					c.domNode.style.display = "";
					var h;
					if(this.fixedHeight){
						h = this._openSpace;
					}else{
						h = parseInt(c.height || c.domNode.getAttribute("height")); // ScrollableView may have the height property
						if(!h){
							c.domNode.style.height = "";
							h = c.domNode.offsetHeight;
							c.domNode.style.height = "0px";
						}
					}
					this.defer(function(){ // necessary for webkitTransition to work
						c.domNode.style.height = h + "px";
					});
					this.select(pane);
				}else if(this.singleOpen){
					this.collapse(c, noAnimation);
				}
			}, this);
			this._updateLast();
			domAttr.set(pane.domNode, "aria-expanded", "true"); // A11Y
			domAttr.set(pane.domNode, "aria-hidden", "false"); // A11Y
		},

		collapse: function(/*Widget*/pane, /*boolean*/noAnimation){
			// summary:
			//		Collapses the given pane to close it.
			// pane:
			//		A pane widget to collapse.
			// noAnimation:
			//		If true, the pane collapses immediately without animation effect.
			if(pane.domNode.style.display === "none"){ return; } // already collapsed
			pane.domNode.style[css3.name("transition")] = noAnimation ? "" : "height "+this.duration+"s linear";
			pane.domNode.style.height = "0px";
			if(!has("css3-animations") || noAnimation){
				pane.domNode.style.display = "none";
				this._updateLast();
			}else{
				// Adding a webkitTransitionEnd handler to panes may cause conflict
				// when the panes already have the one. (e.g. ScrollableView)
				var _this = this;
				_this.defer(function(){
					pane.domNode.style.display = "none";
					_this._updateLast();

					// Need to call parent view's resize() especially when the Accordion is
					// on a ScrollableView, the ScrollableView is scrolled to
					// the bottom, and then expand any other pane while in the
					// non-fixed singleOpen mode.
					if(!_this.fixedHeight && _this.singleOpen){
						for(var v = _this.getParent(); v; v = v.getParent()){
							if(domClass.contains(v.domNode, "mblView")){
								if(v && v.resize){ v.resize(); }
								break;
							}
						}
					}
				}, this.duration*1000);
			}
			this.deselect(pane);
			domAttr.set(pane.domNode, "aria-expanded", "false"); // A11Y
			domAttr.set(pane.domNode, "aria-hidden", "true"); // A11Y
		},

		select: function(/*Widget*/pane){
			// summary:
			//		Highlights the title bar of the given pane.
			// pane:
			//		A pane widget to highlight.
			pane._at.set("selected", true);
			domAttr.set(pane._at.textBoxNode, "aria-selected", "true"); // A11Y
		},

		deselect: function(/*Widget*/pane){
			// summary:
			//		Unhighlights the title bar of the given pane.
			// pane:
			//		A pane widget to unhighlight.
			pane._at.set("selected", false);
			domAttr.set(pane._at.textBoxNode, "aria-selected", "false"); // A11Y
		}
	});
	
	Accordion.ChildWidgetProperties = {
		// summary:
		//		These properties can be specified for the children of a dojox/mobile/Accordion.

		// alt: String
		//		The alternate text of the Accordion title.
		alt: "",
		// label: String
		//		The label of the Accordion title.
		label: "",
		// icon1: String
		//		The unselected icon of the Accordion title.
		icon1: "",
		// icon2: String
		//		The selected icon of the Accordion title.
		icon2: "",
		// iconPos1: String
		//		The position ("top,left,width,height") of the unselected aggregated icon of the Accordion title.
		iconPos1: "",
		// iconPos2: String
		//		The position ("top,left,width,height") of the selected aggregated icon of the Accordion title.
		iconPos2: "",
		// selected: Boolean
		//		The selected state of the Accordion title.
		selected: false,
		// lazy: Boolean
		//		Specifies that the Accordion child must be lazily loaded.
		lazy: false
	};

	// Since any widget can be specified as an Accordion child, mix ChildWidgetProperties
	// into the base widget class.  (This is a hack, but it's effective.)
	// This is for the benefit of the parser.   Remove for 2.0.  Also, hide from doc viewer.
	lang.extend(WidgetBase, /*===== {} || =====*/ Accordion.ChildWidgetProperties);

	return has("dojo-bidi") ? declare("dojox.mobile.Accordion", [Accordion, BidiAccordion]) : Accordion;
});
