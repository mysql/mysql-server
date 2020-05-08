define(["dojox/mobile/ProgressIndicator", "dojox/mobile/TransitionEvent", "dojox/mvc/getStateful"], function(ProgressIndicator, TransitionEvent, getStateful){
	return function(){
		// the default select_item is 0, or will throw an error if directly transition to #details,EditTodoItem view
		this.selected_item = 0;
		this.selected_configuration_item = 0;
		this.progressIndicator = null;
		/*
	 	* show or hide global progress indicator
	 	*/
	 	
	 	// show an item detail
		this.setDetailsContext = function(index){
			if(this.loadedModels.repeatmodels) {
				this.loadedModels.repeatmodels.set("cursorIndex", index);
			}		
		};

		// global for call from template
		this.removeScrollableItem = function(index){
				this.loadedModels.repeatmodels.model.splice(index, 1);
				return false; 	 		
		};

		// insert an item
		this.insertResult = function(index, e){
			var repeatmodel = this.loadedModels.repeatmodels;
			if(index<0 || index>repeatmodel.model.length){
				throw Error("index out of data model.");
			}
			if((repeatmodel.model[index].First=="") ||
				(repeatmodel.model[index+1] && (repeatmodel.model[index+1].First == ""))){
				return;
			}
			var data = {id:Math.random(), "First": "", "Last": "", "Location": "CA", "Office": "", "Email": "", "Tel": "", "Fax": ""};
			repeatmodel.model.splice(index+1, 0, new getStateful(data));
			this.setDetailsContext(index+1);
			var transOpts = {
				title : "repeatDetails",
				target : "repeatDetails",
				url : "#repeatDetails", // this is optional if not set it will be created from target   
				params : {"cursor":index+1}
			};
			new TransitionEvent(e.target, transOpts, e).dispatch(); 
			
		};

	 	
		this.showProgressIndicator = function(show){
				
			if(!this.progressIndicator){
				this.progressIndicator = ProgressIndicator.getInstance({removeOnStop:false, startSpinning:true, size:40, center:true, interval:30});
				// TODO: use dojo.body will throw no appendChild method error.
				var body = document.getElementsByTagName("body")[0];
				body.appendChild(this.progressIndicator.domNode);
				this.progressIndicator.domNode.style.zIndex = 999;
			}
			if(show){
				this.progressIndicator.domNode.style.visibility = "visible";
				this.progressIndicator.start();
			}else{
				this.progressIndicator.stop();
				this.progressIndicator.domNode.style.visibility = "hidden";
			}
			
		}
	};
});
