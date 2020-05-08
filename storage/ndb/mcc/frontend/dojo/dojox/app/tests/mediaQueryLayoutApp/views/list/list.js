define(["dojo/_base/lang", "dojo/dom", "dojo/on", "dijit/registry", "dojo/dom-class"],
function(lang, dom, on, registry, domClass){
	var _onResults = []; // events on array

	return {
	// setDetailsContext for an item
		setDetailsContext: function(index, e, params){
			if(params){
				params.cursor = index;
			}else{
				params = {"cursor":index};
			}
			// transition to itemDetails view with the &cursor=index
			var transOpts = {
				title : "itemDetails",
				target : "itemDetails",
				url : "#itemDetails", // this is optional if not set it will be created from target   
				params : params
			};
			this.app.transitionToView(e.target,transOpts,e);
 
		
		},

	// insert an item
		insertResult: function(index, e){
			if(index<0 || index>this.list.store.data.length){
				throw Error("index out of data model.");
			}
			if((this.list.store.data[index-1].First=="") ||
				(this.list.store.data[index] && (this.list.store.data[index].First == ""))){
				return;
			}
			var data = {id:Math.random(), "label": "", "rightIcon":"mblDomButtonBlueCircleArrow", "First": "", "Last": "", "Location": "CA", "Office": "", "Email": "", "Tel": "", "Fax": ""};
			this.list.store.add(data);
			this.setDetailsContext(index, e);
		},


		// list view init
		init: function(){
			var list = this.list;
			if(!list.Store){
				list.setStore(this.loadedStores.listStore);
			}
		},

		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//

			var onResult;
			
			domClass.remove(dom.byId("listInsert1"), "hide");
			domClass.add(dom.byId("listInsert1"), "show");
			
		//	for(var i = 0; i < this.list.store.data.length; i++){
		//		var item = dom.byId(this.list.store.data[i].id);
		//		onResult = on(item, "click", lang.hitch(this, function(e){
		//			var item = this.list.store.query({"label": e.target.innerHTML})
		//			var index = this.list.store.index[item[0].id];
		//			console.log("index is "+index);
		//			this.setDetailsContext(index, e, this.params);
		//		})); 
		//		_onResults.push(onResult);
		//	}
			
			onResult = this.list.on("click", lang.hitch(this, function(e){
				console.log("List on click hit ",e);
				var item = this.list.store.query({"label": e.target.innerHTML});
				var index = this.list.store.index[item[0].id];
				console.log("index is "+index);
				this.setDetailsContext(index, e, this.params);	
			})); 
			_onResults.push(onResult);
	
			var listInsert1 = dom.byId("listInsert1");
			onResult = on(listInsert1, "click", lang.hitch(this, function(e){
				console.log("listInsert1 on click hit ",e);
				var index = this.list.store.data.length;
				this.insertResult(index, e);
			})); 
			_onResults.push(onResult);
			
		},

		beforeDeactivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//
			domClass.remove(dom.byId("listInsert1"), "show");
			domClass.add(dom.byId("listInsert1"), "hide");
		},
		
		afterDeactivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//
			var onResult = _onResults.pop();
			while(onResult){
				onResult.remove();
				onResult = _onResults.pop();
			}
		},
		
		
		// view destroy, this destroy function can be removed since it is empty
		destroy: function(){
			// _WidgetBase.on listener is automatically destroyed when the Widget itself is. 
		}
	};
});
