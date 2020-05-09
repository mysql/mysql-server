define(["dojo/dom", "dojo/dom-style", "dojo/_base/connect","dijit/registry", "dojo/sniff", "dojox/mobile/TransitionEvent"],
function(dom, domStyle, connect, registry, has, TransitionEvent){
	var app = null;
	var MODULE = "P1";
	return {
		init: function(){
			app = this.app;
		},

		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//console.log(MODULE+" beforeActivate");
		},

		afterActivate: function(){
			// summary:
			//		view life cycle afterActivate()
			//console.log(MODULE+" afterActivate");
		/*
			//loop test
			if(this.app.loopCount++ < ){
				if(history){
					history.back();
				}
			}else{
			//	console.time("timing transition loop");
				console.timeEnd("timing transition loop");
			}
		*/
			if(!this.app.timedAutoFlow && !this.app.timed100Loops){
				return;
			}
			this.app.loopCount++;
			//console.log(MODULE+" afterActivate this.app.loopCount="+this.app.loopCount);
			var liWidget = null;
			if(this.app.timed100Loops){
				if(this.app.loopCount < 100) {
					if(history){
						history.back();
					}
				}else{
					console.log("P1:afterActivate loopCount = 100 stop timer");
					console.timeEnd("timing transition loop");
				}
				return;
			}

			if(this.app.loopCount === 4){
				liWidget = registry.byId("dojox_mobile_ListItem_0"); //P1,S1,V1
			}else if(this.app.loopCount === 5) {
				liWidget = registry.byId("dojox_mobile_ListItem_2"); //P1,S1,V3
			}else if(this.app.loopCount === 6) {
				liWidget = registry.byId("dojox_mobile_ListItem_1"); //V2
			}else if(this.app.loopCount === 8) {
				liWidget = registry.byId("dojox_mobile_ListItem_7"); //P2,S2,Ss2,V5
			}else if(this.app.loopCount === 11) {
			//	liWidget = registry.byId("dojox_mobile_ListItem_6"); //P2,S2,Ss2,V5+P2,S2,Ss2,V6
				liWidget = registry.byId("dojox_mobile_ListItem_3"); //P1,S1,V8
			}else if(this.app.loopCount === 12) {
				liWidget = registry.byId("dojox_mobile_ListItem_4"); //-P1,S1,V8
			//	var liWidget2 = registry.byId("dojox_mobile_ListItem_4"); //-P1,S1,V8
			//	setTimeout(function() {
			//		if(liWidget2){
			//			var ev = new TransitionEvent(liWidget2.domNode, liWidget2.params);
			//		//	ev.dispatch();
			//		}
			//	}, 500);
			}else if(this.app.loopCount === 13) {
				liWidget = registry.byId("dojox_mobile_ListItem_6"); //P2,S2,Ss2,V5+P2,S2,Ss2,V6
			}
			if(liWidget){
				var ev = new TransitionEvent(liWidget.domNode, liWidget.params);
				ev.dispatch();
			}
		},
		beforeDeactivate: function(){
			//console.log(MODULE+" beforeDeactivate");
		},
		afterDeactivate: function(){
			//console.log(MODULE+" afterDeactivate");
		}
	};
});
