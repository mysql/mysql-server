define([], function(){
	var app = null;
	return {
		init: function(){
			app = this.app;
		},

		beforeActivate: function(){
			// summary:
			//		view life cycle beforeActivate()
		},

		afterActivate: function(){
			// summary:
			//		view life cycle afterActivate()
		}
	};
});
