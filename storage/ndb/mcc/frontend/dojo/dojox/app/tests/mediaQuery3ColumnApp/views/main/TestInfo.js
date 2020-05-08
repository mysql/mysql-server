define(["dojo/_base/lang", "dojo/dom-class"],
function(lang, domClass){

	return {
		// view init
		init: function(){
			console.log("in init for view with this.name = "+this.name);

			// handle the backButton click
			this.testheaderBackButton.on("click", lang.hitch(this, function(e){
				this.app.emit("MQ3ColApp/BackFromTest", e);
			})); 
			 
			// This code will setup the classes for the backButton
			domClass.add(this.testheaderBackButton.domNode, "showOnSmall hideOnMedium hideOnLarge");
		},


		afterDeactivate: function(){
			// summary:
			//		view life cycle beforeActivate()
			//
		},

		// view destroy, this destroy function can be removed since it is empty
		destroy: function(){
			// _WidgetBase.on listener is automatically destroyed when the Widget itself is. 
		}
	};
});
