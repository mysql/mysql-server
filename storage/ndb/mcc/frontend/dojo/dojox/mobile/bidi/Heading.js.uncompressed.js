define("dojox/mobile/bidi/Heading", [
	"dojo/_base/declare",
	"./common"
], function(declare, common){

	// module:
	//		dojox/mobile/bidi/Heading

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile Heading widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction support for Label and Back.
		//		This class should not be used directly.
		//		Mobile Heading widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setLabelAttr: function(label){
			this.inherited(arguments);
			if(this.getTextDir(label) === "rtl"){ this.domNode.style.direction = "rtl"; } //for text-overflow: ellipsis;
			this.labelDivNode.innerHTML = common.enforceTextDirWithUcc(this.labelDivNode.innerHTML, this.textDir);
		},

		_setBackAttr: function(back){
			this.inherited(arguments);
			this.backButton.labelNode.innerHTML = common.enforceTextDirWithUcc(this.backButton.labelNode.innerHTML, this.textDir);
			this.labelNode.innerHTML = this.labelDivNode.innerHTML;
		}, 

		_setTextDirAttr: function( textDir){
			if(!this._created || this.textDir != textDir){
				this._set("textDir", textDir);
				if(this.getTextDir(this.labelDivNode.innerHTML) === "rtl"){ this.domNode.style.direction = "rtl"; }//for text-overflow: ellipsis;
				this.labelDivNode.innerHTML = common.enforceTextDirWithUcc(common.removeUCCFromText(this.labelDivNode.innerHTML), this.textDir);
				common.setTextDirForButtons(this);
			}
		}
	});
});

