define(["dojo/_base/lang","dojo/_base/declare", "dojo/dom", "dojo/dom-style", "dojo/sniff", "dojo/_base/window", "dojo/_base/config",
		"dojo/dom-class", "dojo/dom-attr", "dojo/dom-construct", "dojox/app/Controller"],
function(lang, declare, dom, domStyle, has, win, config, domClass, domAttr, domConstruct, Controller){
	// module:
	//		dojox/app/tests/mediaQuery3ColumnApp/controllers/NavigationController
	// summary:
	//		Used to handle Navigation for the application 
	//		

	return declare("dojox/app/tests/mediaQuery3ColumnApp/controllers/NavigationController", Controller, {

		constructor: function(app){
			this.app = app;
			// large > 860 medium <= 860  small <= 560 
			this.small = 560;
			this.medium = 860;
	
			this.lastTransition = "";
			this.lastParams = "";
			this.lastEvent = null;
			this.lastSize = "";
			
			this.events = {
				"MQ3ColApp/TestOption1": this.TestOption1,
				"MQ3ColApp/MainOption1": this.MainOption1,
				"MQ3ColApp/MainOption2": this.MainOption2,
				"MQ3ColApp/MainOption3": this.MainOption3,
				"MQ3ColApp/LastOption1": this.LastOption1,
				"MQ3ColApp/LastOption2": this.LastOption2,
				"MQ3ColApp/LastOption3": this.LastOption3,
				"MQ3ColApp/BackFromMain": this.BackFromMain,
				"MQ3ColApp/BackFromTest": this.BackFromTest,
				"MQ3ColApp/BackFromLast": this.BackFromLast
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
		
		TestOption1: function(e){
			this.doTransition(e, "navLeft","TestInfo","lastRight", null, null);
		},
		MainOption1: function(e){
			var params = {"mainCenter":{'mainSel':"MainOption1","tparam1":"tValue1"}};
			console.log("in NavigationController MainOption1 called.");
			this.doTransition(e, "navLeft","mainCenter","lastRight", params, false);
		},
		MainOption2: function(e){
			var params = {"mainCenter2":{'mainSel':"MainOption2"}};
			console.log("in NavigationController MainOption2 called.");
			this.doTransition(e, "navLeft","mainCenter2","lastRight", params, false);
		},
		MainOption3: function(e){
			var params = {"mainCenter3":{'mainSel':"MainOption3"}};
			console.log("in NavigationController MainOption3 called.");
			this.doTransition(e, "navLeft","mainCenter3","lastRight", params, false);
		},
		LastOption1: function(e){
			var params = lang.mixin(this.lastParams,{'lastSel':"LastOption1"});
			if(!this.isLarge()){
				console.log("in NavigationController LastOption1 called with isMedium or isSmall.");
				this.doTransition(e, "navLeft","lastCenter","lastRight", params, false);
			}else{
				console.log("in NavigationController LastOption1 called with isLarge.");
				this.doTransition(e, "navLeft", this._getMainCenter(),"lastRight", params, false);
			}
		},
		LastOption2: function(e){
			var params = lang.mixin(this.lastParams,{'lastSel':"LastOption2"});
			if(!this.isLarge()){
				console.log("in NavigationController LastOption2 called with isMedium or isSmall.");
				this.doTransition(e, "navLeft","lastCenter","lastRight", params, false);
			}else{
				console.log("in NavigationController LastOption2 called with isLarge.");
				this.doTransition(e, "navLeft", this._getMainCenter(),"lastRight", params, false);
			}
		},
		LastOption3: function(e){
			var params = lang.mixin(this.lastParams,{'lastSel':"LastOption3"});
			if(!this.isLarge()){
				console.log("in NavigationController LastOption3 called with isMedium or isSmall.");
				this.doTransition(e, "navLeft","lastCenter","lastRight", params, false);
			}else{	
				console.log("in NavigationController LastOption2 called with isLarge.");
				this.doTransition(e, "navLeft", this._getMainCenter(),"lastRight", params, false);
			}
		},

		// Called to get the correct mainCenter
		_getMainCenter: function(){
			if(this.lastCenter == "mainCenter" || this.lastCenter == "mainCenter2" || this.lastCenter == "mainCenter3"){
				return this.lastCenter;
			}
			return "mainCenter";

		},
		// These BackFrom ones need work to really go back in the history.
		BackFromMain: function(e){
			if(this.isSmall()){
				console.log("in NavigationController BackFromMain called with isSmall.");
				// not sure about using his.lastParams
				this.doTransition(e, "navLeft","navCenter","lastRight", this.lastParams, true);
			}else{
				console.log("in NavigationController BackFromMain called with !isSmall.");
				this.doTransition(e, "mainCenter","navLeft","lastRight", this.lastParams, true);
			}
		},
		BackFromTest: function(e){
			if(this.isSmall()){
				console.log("in NavigationController BackFromTest called with isSmall.");
				this.doTransition(e, "navLeft","navCenter","lastRight", this.lastParams, true);
			}else{
				console.log("in NavigationController BackFromTest called with !isSmall.");
				this.doTransition(e, "navLeft","mainCenter","lastRight", this.lastParams, true);
			}
		},
		BackFromLast: function(e){
			console.log("in NavigationController BackFromLast called.");
			this.doTransition(e, "navLeft","mainCenter","lastRight", this.lastParams, true);
		},

		doTransition: function(e, left, center, right, params, back){
			this.lastLeft = left;
			this.lastCenter = center;
			this.lastRight = right;
			this.lastParams = params;
			var views = this.lastTransition = left+"+"+center;
			if(right !== "-lastRight"){
				views = this.lastTransition = views+"+"+right;
			}else{
				views = this.lastTransition = views+right;
			}
			console.log("in NavigationController views = "+views+"  this.params="+this.params);
			
			this.lastParams = params;
			this.lastEvent = e;
			var transOpts = {
				title: views,
				target: views,
				url: "#"+views,
				reverse: back,
			//	"transition": "none",
				params:params
			};
			this.app.transitionToView(e.target,transOpts,e);
		},

	
		
		onResize: function(){
			// this is called on a resize, normally we want to use css to adjust, but a resize or an orientation change
			// causes us to be showing duplicate views, then we need to update the views being shown.
			
			if(this.lastSize == this.getSize()){
				return;
			}
			this.lastSize = this.getSize();
			
			// for a large screen we should always be showing "navLeft+mainCenter+lastRight"
			if(this.isLarge() && this.lastEvent && 
				(this.lastTransition !== "navLeft+mainCenter+lastRight" && this.lastTransition !== "navLeft+TestInfo+lastRight")){
				console.log("in NavigationController onResize isLarge calling transition with navLeft+mainCenter+lastRight");
				this.doTransition(this.lastEvent, "navLeft","mainCenter","lastRight", this.lastParams);
			}else if(this.isMedium() && this.lastEvent){
				// for a medium screen we need to check for "navLeft+navCenter+lastRight" and for "navLeft+lastCenter+lastRight"
				if(this.lastTransition == "navLeft+navCenter+lastRight"){
					console.log("in NavigationController onResize isMedium and navCenter calling transition with navLeft+mainCenter+lastRight");
					this.doTransition(this.lastEvent, "navLeft","mainCenter","lastRight", this.lastParams);
				}else if(this.lastTransition == "navLeft+lastCenter+lastRight"){
					console.log("in NavigationController onResize isMedium and lastCenter calling transition with navLeft+LastCenter+lastRight");
					this.doTransition(this.lastEvent, "navLeft","lastCenter","lastRight", this.lastParams);
				}
			}

		},

		getSize: function(){
			if(this.isLarge()){
				return "large";
			}else if(this.isMedium()){
				return "medium";
			}else{
				return "small";
			}
		},
		
		// large > 860 medium <= 860  small <= 560 
		isLarge: function(){
			var width = window.innerWidth || document.documentElement.clientWidth;
			return width > this.medium;

		},

		isMedium: function(){
			var width = window.innerWidth || document.documentElement.clientWidth;
			return width <= this.medium && width > this.small;

		},

		isSmall: function(){
			var width = window.innerWidth || document.documentElement.clientWidth;
			return width <= this.small;

		}
		
	});
});
