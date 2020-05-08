define(["dojo/dom", "dojo/dom-style", "dojo/_base/connect", "dojox/mobile/TransitionEvent"],
function(dom, domStyle, connect, TransitionEvent){
	var _connectResults = []; // events connect result

	return {
		// view init
		init: function(){
		},
		
		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()

			var connectResult;
			
			var backButtomDom = dom.byId('headerBackButton');
			connectResult = connect.connect(backButtomDom, "onclick", function(e){
				
				var transOpts = {
					title:'header+navigation+centerNavigation',
					target:'header+navigation+centerNavigation',
					url:'#header+navigation+centerNavigation'					
				};
				new TransitionEvent(e.target, transOpts, e).dispatch(); 

			});
			_connectResults.push(connectResult);
			
		},


		afterDeactivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//
			var connectResult = _connectResults.pop();
			while(connectResult){
				connect.disconnect(connectResult);
				connectResult = _connectResults.pop();
			}
		},
		
		// view destroy
		destroy: function(){
			var connectResult = _connectResults.pop();
			while(connectResult){
				connect.disconnect(connectResult);
				connectResult = _connectResults.pop();
			}
		}
	};
});
