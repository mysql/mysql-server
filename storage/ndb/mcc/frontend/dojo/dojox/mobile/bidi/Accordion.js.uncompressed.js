define("dojox/mobile/bidi/Accordion", [
	"dojo/_base/declare",
	"./common",
	"dojo/dom-class"
], function(declare, common, domClass){

	// module:
	//		dojox/mobile/bidi/Accordion

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile Accordion widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction support for Label.
		//		This class should not be used directly.
		//		Mobile Accordion widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setupChild: function(child){
			if(this.textDir){
				child.label = common.enforceTextDirWithUcc(child.label, this.textDir); 
			}
			this.inherited(arguments);
		},
		_setIconDir: function(iconNode){
			domClass.add(iconNode, "mblAccordionIconParentRtl");
		}
	});
});
