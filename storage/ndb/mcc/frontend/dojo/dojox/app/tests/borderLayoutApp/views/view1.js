define([], function(){

	return {
		init: function(){
			console.log("In view1 init called");
		},

		beforeActivate: function(){
			console.log("In view1 beforeActivate called");
		},

		afterActivate: function(){
			console.log("In view1 afterActivate called");
		},

		beforeDeactivate: function(){
			console.log("In view1 beforeDeactivate called");
		},

		afterDeactivate: function(){
			console.log("In view1 afterDeactivate called");
		}
	}
});
