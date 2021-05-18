define("dojox/mobile/bidi/ProgressIndicator", [
	"dojo/_base/declare",
	"dojo/dom-class"
], function(declare, domClass){

	// module:
	//		dojox/mobile/bidi/SimpleDialog

	return declare(null, {

		buildRendering:function(){
			this.inherited(arguments);
			if(!this.isLeftToRight()){
				if(this.closeButton){
					var s = Math.round(this.closeButtonNode.offsetHeight / 2);
					this.closeButtonNode.style.left = -s + "px";
				}
				if(this.center){
					domClass.add(this.domNode, "mblProgressIndicatorCenterRtl");
				}
				domClass.add(this.containerNode, "mblProgContainerRtl");
			}
		}

	});
});

