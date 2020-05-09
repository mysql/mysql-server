define([],function(){

	var app = null;

	return {
		init: function(){
			app = this.app;
		},
		
		beforeActivate: function(){
			app.stopTransition = false;
		},
		
		afterActivate: function(){
		},
		
		beforeDeactivate: function(){
		},

		afterDeactivate: function(){
		}
	};
});
