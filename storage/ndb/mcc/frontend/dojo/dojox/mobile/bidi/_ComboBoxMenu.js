define([
	"dojo/_base/declare",
	"dojo/dom-construct",
	"dojo/dom-class",
	"dojo/dom-style"
],
	function(declare, domConstruct, domClass, domStyle){
	// module:
	//		dojox/mobile/bidi/_ComboBoxMenu

	return declare(null, {

		buildRendering: function(){
			this.inherited(arguments);
			// dojox.mobile mirroring support
			if(!this.isLeftToRight()){
				this.containerNode.style.left = "auto";
				domStyle.set(this.containerNode, { position:"absolute", top:0, right:0 });
				domClass.remove(this.previousButton, "mblComboBoxMenuItem");
				domClass.add(this.previousButton, "mblComboBoxMenuItemRtl");
				domClass.remove(this.nextButton, "mblComboBoxMenuItem");
				domClass.add(this.nextButton, "mblComboBoxMenuItemRtl");
			}
		}
		
	});
});
