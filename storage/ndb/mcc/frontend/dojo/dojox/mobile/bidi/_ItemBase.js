define([
	"dojo/_base/declare",
	"./common"
], function(declare, common){

	// module:
	//		dojox/mobile/bidi/_ItemBase

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile _ItemBase widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction support for Label.
		//		This class should not be used directly.
		//		Mobile _ItemBase loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setLabelAttr: function(/*String*/ text){
			this._set("label", text);
			this.labelNode.innerHTML = this._cv ? this._cv(text) : text;
			if (!this.textDir){
				var p = this.getParent();
				this.textDir = p && p.get("textDir") ? p.get("textDir") : "";
			}
			this.labelNode.innerHTML = common.enforceTextDirWithUcc(this.labelNode.innerHTML, this.textDir);
		},
		_setTextDirAttr: function(/*String*/ textDir){
			if(!this._created || this.textDir !== textDir){
				this._set("textDir", textDir);
				this.labelNode.innerHTML = common.enforceTextDirWithUcc(common.removeUCCFromText(this.labelNode.innerHTML), this.textDir);
				if(this.badgeObj && this.badgeObj.setTextDir){ this.badgeObj.setTextDir(textDir); }
			}
		},
		getTransOpts: function(){
			var opts = this.inherited(arguments);
			if(!this.isLeftToRight()){
				opts.transitionDir = opts.transitionDir * -1; 
			}
			return opts;
		}		
	});
});
