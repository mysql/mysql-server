define([], function(){

	var repeatmodel = null;	//repeat view data model

	// show an item detail
	var setDetailsContext = function(index){
		// only set the cursor if it is different and valid
		if(parseInt(index) != repeatmodel.cursorIndex && parseInt(index) < repeatmodel.model.length){
			repeatmodel.set("cursorIndex", parseInt(index));
		}
	};

	return {
		// repeat view init
		init: function(){
			repeatmodel = this.loadedModels.repeatmodels;
		},

		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//
			// if this.params["cursor"] is set use it to set the selected Details Context
			if(this.params["cursor"]){
				setDetailsContext(this.params["cursor"]);
			}
		}
	}
});
