define(["dojo/_base/lang", "dojo/dom-class"],
function(lang, domClass){

	return {
		// view init
		init: function(){
			console.log("in init for view with this.name = "+this.name);

			// handle the backButton click
			this.lastheaderBackButton.on("click", lang.hitch(this, function(e){
				if(history){
					history.back();
				}else{
					this.app.emit("MQ3ColApp/BackFromLast", e);
				}
			})); 

			// This code will setup the view to work in the center or the right depending upon the viewName
			if(this.name == "lastCenter"){
				this.lastOuterContainer["data-app-constraint"] = "center";
				domClass.add(this.lastheaderBackButton.domNode, "showOnMedium hideOnLarge showOnSmall");
				domClass.add(this.lastOuterContainer, "center");
			}else{
				this.lastOuterContainer["data-app-constraint"] = "right";				
				domClass.add(this.lastheaderBackButton.domNode, "showOnSmall hideOnMedium hideOnLarge");
				domClass.add(this.lastOuterDiv, "navPane hideOnMedium");
				domClass.add(this.lastOuterContainer, "hideOnMedium right");
			}


		},
		
		beforeActivate: function(previousView, data){
			// summary:
			//		view life cycle beforeActivate()
			//
			this.previousView = previousView;

			// Set the selection from the params
			if(this.params["lastSel"]){ 
				this.lastH2.set("label",this.params["lastSel"]+" selected");
			}else{
				this.lastH2.set("label","None selected");				
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
