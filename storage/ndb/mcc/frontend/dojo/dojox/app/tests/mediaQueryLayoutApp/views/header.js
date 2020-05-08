define(["dojo/_base/lang", "dojo/dom", "dojo/on", "dijit/registry", "dojo/date/stamp", "dojox/app/utils/constraints", "dojox/mobile/TransitionEvent"], 
function(lang, dom, on, registry, stamp, constraints, TransitionEvent){
	var _onResults = []; // events on array


	return {
		init: function(){
			//console.log("date view beforeActivate()");
			var backButtomDom = dom.byId('headerBackButton');
			var onResult = on(backButtomDom, "click", lang.hitch(this, function(e){

				if(this.app.children.mediaQueryLayoutApp_itemDetails && this.app.children.mediaQueryLayoutApp_itemDetails.viewShowing){
					var transOpts = {
						title:'header+navigation+listMain',
						target:'header+navigation+listMain',
						url:'#header+navigation+listMain'					
					};
					new TransitionEvent(e.target, transOpts, e).dispatch();
				}else{			
					var transOpts = {
						title:'header+navigation+centerNavigation',
						target:'header+navigation+centerNavigation',
						url:'#header+navigation+centerNavigation'					
					};
					new TransitionEvent(e.target, transOpts, e).dispatch();
				} 
			})); 
			_onResults.push(onResult);			
		},

		beforeActivate: function(){
		},


		// view destroy
		destroy: function(){
			var onResult = _onResults.pop();
			while(onResult){
				onResult.remove();
				onResult = _onResults.pop();
			}
		}
	};
	
});
