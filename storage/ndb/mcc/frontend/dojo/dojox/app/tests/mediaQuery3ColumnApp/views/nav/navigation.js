define(["dojo/dom-class"],
function(domClass){
	return {
		// view init
		init: function(){
			console.log("in init for view with this.name = "+this.name);

			// the back button is never shown for Nav
			domClass.add(this.navheaderBackButton.domNode, "hide");

			// This code will setup the view to work in the left or center depending upon the view name
			if(this.name == "navLeft"){
				this.navOuterContainer["data-app-constraint"] = "left";
				domClass.add(this.navOuterDiv, "hideOnSmall");  // for navigation on the left
			}else{
				this.navOuterContainer["data-app-constraint"] = "center";				
				domClass.add(this.navOuterContainer, "center");
			}

		},
		
		beforeActivate: function(previousView){
			// summary:
			//		view life cycle beforeActivate()
			//
			this.previousView = previousView;
			
		},

		beforeDeactivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//
		},

		testOption1Clicked: function(/*Event*/ e){
			e.transitionNext = e.target.parentElement.id;
			this.app.emit("MQ3ColApp/TestOption1", e);
		},

		mainOption1Clicked: function(/*Event*/ e){
			this.app.emit("MQ3ColApp/MainOption1", e);
		},

		mainOption2Clicked: function(/*Event*/ e){
			this.app.emit("MQ3ColApp/MainOption2", e);
		},

		mainOption3Clicked: function(/*Event*/ e){
			this.app.emit("MQ3ColApp/MainOption3", e);
		}

	}
});
