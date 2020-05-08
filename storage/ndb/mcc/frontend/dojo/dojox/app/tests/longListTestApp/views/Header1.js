define(["dojo/dom", "dojo/dom-style", "dojo/_base/connect", "dojo/store/Memory", "dojo/store/Observable", "dojo/sniff"],
function(dom, domStyle, connect, Memory, Observable, has){

		var _connectResults = []; // events connect result
		var backId = 'sc1back1';
		var insert10Id = 'sc1insert10x';
		var app = null;
		
		var loadMore = function(){
			if(!app){
				return;
			}
			if(!app.listStart){
				app.listStart = 1;
				app.listCount = 5;
			}
			setTimeout(function(){ // to simulate network latency
				for(var i = app.listStart; i < app.listStart+5; i++){
					var newdata = {'label': 'Item #'+i};
					app.stores.longlistStore.store.put(newdata);
				}
				app.listStart += app.listCount;
				app.listTotal = app.listStart-1;				
				return false;
			}, 500);
		};
	return {
		init: function(){
			app = this.app;
			var memoryStore = new Memory({data: {}});
			new Observable(memoryStore);

			var connectResult;
			connectResult = connect.connect(dom.byId(insert10Id), "click", function(){
				//Add 10 items to the end of the model
				loadMore();
			});
			_connectResults.push(connectResult);
		},


		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			if(dom.byId(backId) && !has("phone")){ 
				domStyle.set(dom.byId(backId), "visibility", "hidden"); // hide the back button in tablet mode
			}
			if(dom.byId("tab1WrapperA")){ 
				domStyle.set(dom.byId("tab1WrapperA"), "visibility", "visible");  // show the nav view if it being used
				domStyle.set(dom.byId("tab1WrapperB"), "visibility", "visible");  // show the nav view if it being used
			}
		},

		afterActivate: function(){
			// summary:
			//		view life cycle afterActivate()
		},
				
		destroy: function(){
			var connectResult = _connectResults.pop();
			while(connectResult){
				connect.disconnect(connectResult);
				connectResult = _connectResults.pop();
			}
		}
	};
});
