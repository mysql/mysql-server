define([
	"dojo/_base/declare",
	"./common"
], function(declare, common){

	// module:
	//		mobile/bidi/TabBar

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile TabBar widget, using Unicode Control Characters to control text direction.
		// description:
		//		Attribute textDir is set to TabBar and to TabBarButtons.
		//		This class should not be used directly.
		//		Mobile TabBar widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setTextDirAttr: function(/*String*/ textDir){
			if(!this._created || this.textDir !== textDir){
				this._set("textDir", textDir);
				common.setTextDirForButtons(this);
			}
		}
	});
});
