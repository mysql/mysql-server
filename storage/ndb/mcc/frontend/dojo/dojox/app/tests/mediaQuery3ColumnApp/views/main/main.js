define(["dojo/_base/lang", "dojo/dom-class"],
function(lang, domClass){

	return {
		// view init
		init: function(){
			console.log("in init for view with this.name = "+this.name);

			// handle the backButton click
			this.mainheaderBackButton.on("click", lang.hitch(this, function(e){
				if(history){
					history.back();
				}else{
					this.app.emit("MQ3ColApp/BackFromMain", e);
				}
			})); 

			// This code will setup the view to work in the left or center depending upon the view name
			if(this.name == "mainLeft"){  // app was changed to stop using mainLeft for now.
				this.mainOuterContainer["data-app-constraint"] = "left";
				domClass.add(this.mainheaderBackButton.domNode, "showOnMedium");
				domClass.add(this.mainOuterContainer, "left");
				domClass.add(this.mainOuterDiv, "navPane hideOnSmall");  // for main on left
			}else{
				this.mainOuterContainer["data-app-constraint"] = "center";				
				domClass.add(this.mainheaderBackButton.domNode, "showOnSmall hideOnMedium hideOnLarge");
				domClass.add(this.mainOuterContainer, "center");
			}


		},
		
		beforeActivate: function(previousView, data){
			// summary:
			//		view life cycle beforeActivate()
			//
			this.previousView = previousView;
			
			// Set the selection from the params
			if(this.params["mainSel"]){ 
				if(this.mainH2){
					this.mainH2.set("label",this.params["mainSel"]+" selected");
				}else{
					console.error("Problem this.mainH2 should not be null ");
				}	
			}else{
				this.mainH2.set("label","None selected");				
			}
		},

		beforeDeactivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//
		},

		lastOption1Clicked: function(/*Event*/ e){
			this.app.emit("MQ3ColApp/LastOption1", e);
		},

		lastOption2Clicked: function(/*Event*/ e){
			this.app.emit("MQ3ColApp/LastOption2", e);
		},

		lastOption3Clicked: function(/*Event*/ e){
			this.app.emit("MQ3ColApp/LastOption3", e);
		},

		// view destroy, this destroy function can be removed since it is empty
		destroy: function(){
			// _WidgetBase.on listener is automatically destroyed when the Widget itself is. 
		}
	};
});
