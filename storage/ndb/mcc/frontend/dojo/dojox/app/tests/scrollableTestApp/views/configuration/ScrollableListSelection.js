define([], function(){

	var app = null;

	return {
		init: function(){
			app = this.app;
		},
		
		beforeActivate: function(){
			app.stopTransition = false;
			//console.log("configuration/ScrollableListSelection beforeActivate called this.app.selected_configuration_item=",this.app.selected_configuration_item);
		},
		
		afterActivate: function(){
			//console.log("configuration/ScrollableListSelection afterActivate called this.app.selected_configuration_item=",this.app.selected_configuration_item);
			//console.log("setting configurewrapper visible 1");
			//domStyle.set(dom.byId("configurewrapper"), "visibility", "visible"); // show the items list
		},
		
		beforeDeactivate: function(){
			//console.log("configuration/ScrollableListSelection beforeDeactivate called this.app.selected_configuration_item=",this.app.selected_configuration_item);
		},

		afterDeactivate: function(){
			//console.log("configuration/ScrollableListSelection afterDeactivate called this.app.selected_configuration_item=",this.app.selected_configuration_item);
			//console.log("setting configurewrapper hidden");
			//domStyle.set(dom.byId("configurewrapper"), "visibility", "hidden"); // hide the items list 
		}
	}
});
