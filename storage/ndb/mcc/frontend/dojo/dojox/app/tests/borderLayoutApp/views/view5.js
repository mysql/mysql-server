define([], function(){

	return {
		init: function(){
			console.log("In view5 init called");
		},

		beforeActivate: function(){
			console.log("In view5 beforeActivate called");
		},

		afterActivate: function(){
			console.log("In view5 afterActivate called");
		},

		beforeDeactivate: function(){
			console.log("In view5 beforeDeactivate called");
		},

		afterDeactivate: function(){
			console.log("In view5 afterDeactivate called");
		}
	}
});
