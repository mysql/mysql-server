define([
	"dojo/_base/declare",
	"./common"
], function(declare, common){

	// module:
	//		dojox/mobile/bidi/IconMenu

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile IconMenu widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction, textDir is set to MenuItems.
		//		This class should not be used directly.
		//		Mobile IconMenu widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setTextDirAttr: function(/*String*/ textDir){
			if(!this._created || this.textDir !== textDir){
				this._set("textDir", textDir);
				common.setTextDirForButtons(this);
			}
		}
	});
});
