define("dojox/mobile/bidi/Button", ["dojo/_base/declare", "./common"], function(declare, common){

	// module:
	//		mobile/bidi/Button

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile Button widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction support for Label.
		//		This class should not be used directly.
		//		Mobile Button widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setLabelAttr: function(/*String*/ content){
			this.inherited(arguments, [this._cv ? this._cv(content) : content]);
			this.focusNode.innerHTML = common.enforceTextDirWithUcc(this.focusNode.innerHTML, this.textDir); 
		},

		_setTextDirAttr: function(/*String*/ textDir){
			if(!this._created || this.textDir !== textDir){
				this._set("textDir", textDir);
				this.focusNode.innerHTML = common.enforceTextDirWithUcc(common.removeUCCFromText(this.focusNode.innerHTML), this.textDir);
			}
		}
	});
});
