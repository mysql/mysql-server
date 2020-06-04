define("dojox/mobile/bidi/SimpleDialog", [
	"dojo/_base/declare"
], function(declare){

	// module:
	//		dojox/mobile/bidi/SimpleDialog

	return declare(null, {

		refresh:function(){
			this.inherited(arguments);
			if(!this.isLeftToRight() && this.closeButton){
				var s = Math.round(this.closeButtonNode.offsetHeight / 2);
				this.closeButtonNode.style.left = -s + "px";
			}
		}

	});
});

