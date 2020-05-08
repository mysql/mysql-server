define([
	"dojo/_base/declare",
	"dojo/dom-class"
], function(declare, domClass){

	// module:
	//		mobile/bidi/FormLayout

	return declare(null, {

		buildRendering:function(){
			this.inherited(arguments);
			if(!this.isLeftToRight() && this.rightAlign){
				domClass.add(this.domNode, "mblFormLayoutRightAlignRtl");
			}
		}
	});
});
