define([
	"dojo/_base/declare",
	"./common"
], function(declare, common){

	// module:
	//		dojox/mobile/bidi/RoundRectCategory

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile RoundRectCategory widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction support for Label.
		//		This class should not be used directly.
		//		Mobile RoundRectCategory widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setLabelAttr: function(text){
			if(this.textDir){
				text = common.enforceTextDirWithUcc(text, this.textDir);
			}
			this.inherited(arguments);
		},
		_setTextDirAttr: function(textDir){
			if(textDir && this.textDir !== textDir){
				this.textDir = textDir;
				this.label = common.removeUCCFromText(this.label);
				this.set('label', this.label);
			}
		}
	});
});
