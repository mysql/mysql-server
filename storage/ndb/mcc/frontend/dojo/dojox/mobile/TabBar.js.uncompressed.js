define("dojox/mobile/TabBar", [
	"dojo/_base/array",
	"dojo/_base/declare",
	"dojo/_base/window",
	"dojo/dom-class",
	"dojo/dom-construct",
	"dojo/dom-geometry",
	"dojo/dom-style",
	"dojo/dom-attr",
	"dijit/_Contained",
	"dijit/_Container",
	"dijit/_WidgetBase",
	"./TabBarButton",// to load TabBarButton for you (no direct references)
	"dojo/has",
	"dojo/has!dojo-bidi?dojox/mobile/bidi/TabBar"	
], function(array, declare, win, domClass, domConstruct, domGeometry, domStyle, domAttr, Contained, Container, WidgetBase, TabBarButton, has, BidiTabBar){

	// module:
	//		dojox/mobile/TabBar

	var TabBar =  declare(has("dojo-bidi") ? "dojox.mobile.NonBidiTabBar" : "dojox.mobile.TabBar", [WidgetBase, Container, Contained],{
		// summary:
		//		A bar widget that has buttons to control visibility of views.
		// description:
		//		TabBar is a container widget that has typically multiple
		//		TabBarButtons which controls visibility of views. It can be used
		//		as a tab container.

		// iconBase: String
		//		The default icon path for child items.
		iconBase: "",

		// iconPos: String
		//		The default icon position for child items.
		iconPos: "",

		// barType: String
		//		"tabBar", "segmentedControl", "standardTab", "slimTab", "flatTab",
		//		or "tallTab"
		barType: "tabBar",

		// closable: Boolean
		//		If true, user can close (destroy) a child tab by clicking the X on the tab.
		//		This property is NOT effective for "tabBar" and "tallBar".
		closable: false,

		// center: Boolean
		//		If true, place the tabs in the center of the bar.
		//		This property is NOT effective for "tabBar".
		center: true,

		// syncWithViews: Boolean
		//		If true, this widget listens to view transition events to be
		//		synchronized with view's visibility.
		syncWithViews: false,

		// tag: String
		//		A name of html tag to create as domNode.
		tag: "ul",

		// fill: String
		//		Define if the TabBar should resize its children so that they evenly fill all the available space in the bar.
		//
		//		Allowed values:
		//
		//		1. "auto" is the default behavior from version 1.8: bar buttons are resized to evenly fill the entire bar only on small devices (width < 500px) and when barType is "tabBar"
		//		2. "always": bar buttons are always resized to evenly fill the entire bar
		//		3. "never": bar buttons are never resized to evenly fill the entire bar
		fill: "auto",

		/* internal properties */
		// selectOne: [private] Boolean
		//		Specifies that only one item can be selected.
		selectOne: true,
		baseClass: "mblTabBar",
		_fixedButtonWidth: 76,
		_fixedButtonMargin: 17,
		_largeScreenWidth: 500,

		buildRendering: function(){
			this.domNode = this.srcNodeRef || domConstruct.create(this.tag);
			domAttr.set(this.domNode, "role", "tablist");
			this.reset();
			this.inherited(arguments);
		},

		postCreate: function(){
			if(this.syncWithViews){ // see also RoundRect#postCreate
				var f = function(view, moveTo, dir, transition, context, method){
					var child = array.filter(this.getChildren(), function(w){
						return w.moveTo === "#" + view.id || w.moveTo === view.id; })[0];
					if(child){ child.set("selected", true); }
				};
				this.subscribe("/dojox/mobile/afterTransitionIn", f);
				this.subscribe("/dojox/mobile/startView", f);
			}
		},

		startup: function(){
			if(this._started){ return; }
			this.inherited(arguments);
			this.resize();
		},

		reset: function(){
			// summary:
			//		Resets the widget to its initial state.
			var prev = this._barType;
			if(typeof this.barType === "object"){
				this._barType = this.barType["*"];
				for(var c in this.barType){
					if(domClass.contains(win.doc.documentElement, c)){
						this._barType = this.barType[c];
						break;
					}
				}
			}else{
				this._barType = this.barType;
			}
			var cap = function(s){
				return s.charAt(0).toUpperCase() + s.substring(1);
			};
			if(prev){
				domClass.remove(this.domNode, this.baseClass + cap(prev));
			}
			domClass.add(this.domNode, this.baseClass + cap(this._barType));
		},

		resize: function(size){
			var i, w;
			if(size && size.w){
				w = size.w;
			}else{
				w = domGeometry.getMarginBox(this.domNode).w;
			}
			var bw = this._fixedButtonWidth;
			var bm = this._fixedButtonMargin;
			var arr = array.map(this.getChildren(), function(w){ return w.domNode; });

			domClass.toggle(this.domNode, "mblTabBarNoIcons",
							!array.some(this.getChildren(), function(w){ return w.iconNode1; }));
			domClass.toggle(this.domNode, "mblTabBarNoText",
							!array.some(this.getChildren(), function(w){ return w.label; }));

			var margin = 0;
			if(this._barType == "tabBar"){
				this.containerNode.style.paddingLeft = "";
				margin = Math.floor((w - (bw + bm * 2) * arr.length) / 2);
				if(this.fill == "always" || (this.fill == "auto" && (w < this._largeScreenWidth || margin < 0))){
					domClass.add(this.domNode, "mblTabBarFill");
					for(i = 0; i < arr.length; i++){
						arr[i].style.width = (100/arr.length) + "%";
						arr[i].style.margin = "0";
					}
				}else{
					// Fixed width buttons. Mainly for larger screen such as iPad.
					for(i = 0; i < arr.length; i++){
						arr[i].style.width = bw + "px";
						arr[i].style.margin = "0 " + bm + "px";
					}
					if(arr.length > 0){
						if(has("dojo-bidi") && !this.isLeftToRight()){
							arr[0].style.marginLeft = "0px";
							arr[0].style.marginRight = margin + bm + "px";
						}else{
							arr[0].style.marginLeft = margin + bm + "px";
						}
					}
					this.containerNode.style.padding = "0px";
				}
			}else{
				for(i = 0; i < arr.length; i++){
					arr[i].style.width = arr[i].style.margin = "";
				}
				var parent = this.getParent();
				if(this.fill == "always"){
					domClass.add(this.domNode, "mblTabBarFill");
					for(i = 0; i < arr.length; i++){
						arr[i].style.width = (100/arr.length) + "%";
						if(this._barType != "segmentedControl" && this._barType != "standardTab") {
							arr[i].style.margin = "0";
						}
					}
				}else{
					if(this.center && (!parent || !domClass.contains(parent.domNode, "mblHeading"))){
						margin = w;
						for(i = 0; i < arr.length; i++){
							margin -= domGeometry.getMarginBox(arr[i]).w;
						}
						margin = Math.floor(margin/2);
					}
					if(has("dojo-bidi") && !this.isLeftToRight()){
						this.containerNode.style.paddingLeft = "0px";
						this.containerNode.style.paddingRight = margin ? margin + "px" : "";
					}else{
						this.containerNode.style.paddingLeft = margin ? margin + "px" : "";
					}
				}
			}
			if(size && size.w){
				domGeometry.setMarginBox(this.domNode, size);
			}
		},
		getSelectedTab: function(){
			// summary:
			//		Returns the first selected child.
			return array.filter(this.getChildren(), function(w){ return w.selected; })[0];
		},

		onCloseButtonClick: function(/*TabBarButton*/tab){
			// summary:
			//		Called whenever the close button [X] of a child tab is clicked.
			return true;
		}
	});
	
	return has("dojo-bidi")?declare("dojox.mobile.TabBar", [TabBar, BidiTabBar]):TabBar;	
});
