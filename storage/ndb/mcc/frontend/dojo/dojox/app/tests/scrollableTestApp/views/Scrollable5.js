define(["dojo/dom", "dojo/dom-style", "dojo/_base/connect", "dojo/_base/lang","dijit/registry", "dojox/mvc/at", 
		"dojox/mvc/Repeat", "dojox/mvc/getStateful", "dojox/mvc/Output", "dojo/sniff"],
function(dom, domStyle, connect, lang, registry, at, Repeat, getStateful, Output, has){
	var _connectResults = []; // events connect result

	var repeatmodel = null;	//repeat view data model
	
	// these ids are updated here and in the html file to avoid duplicate ids
	var backId = 'sc5back1';
	var insert1Id = 'sc5insert1x';
	var insert10Id = 'sc5insert10x';
	var remove10Id = 'sc5remove10x';

	var app = null;

	return {
		// repeat view init
		init: function(){
			app = this.app;

			repeatmodel = this.loadedModels.repeatmodels;
			var connectResult;

			connectResult = connect.connect(dom.byId(insert1Id), "click", lang.hitch(this,function(e){
				this.app.insertResult(repeatmodel.model.length-1,e);
			}));
			_connectResults.push(connectResult);

			connectResult = connect.connect(dom.byId(insert10Id), "click", function(){
				//Add 10 items to the end of the model
				app.showProgressIndicator(true);
				setTimeout(lang.hitch(this,function(){
					var maxentries = repeatmodel.model.length+10;
					for(var i = repeatmodel.model.length; i < maxentries; i++){
						var data = {id:Math.random(), "First": "First"+repeatmodel.model.length, "Last": "Last"+repeatmodel.model.length, "Location": "CA", "Office": "", "Email": "", "Tel": "", "Fax": ""};
						repeatmodel.model.splice(repeatmodel.model.length, 0, new getStateful(data));					
					}
					app.showProgressIndicator(false);
				}), 100);				
				
			});
			_connectResults.push(connectResult);

			connectResult = connect.connect(dom.byId(remove10Id), "click", function(){
				//remove 10 items to the end of the model
				app.showProgressIndicator(true);
				setTimeout(lang.hitch(this,function(){				
					var maxentries = repeatmodel.model.length-10;
					for(var i = repeatmodel.model.length; i >= maxentries; i--){
						repeatmodel.model.splice(i, 1);
					}
					repeatmodel.set("cursorIndex", 0);		
					app.showProgressIndicator(false);
				}), 100);				
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
		
		
		destroy: function(){
			var connectResult = _connectResults.pop();
			while(connectResult){
				connect.disconnect(connectResult);
				connectResult = _connectResults.pop();
			}
		}
	};
});
