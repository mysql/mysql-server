define("dojox/mobile/bidi/IconItem", [
	"dojo/_base/declare",
	"./common"
], function(declare, common){

	// module:
	//		dojox/mobile/bidi/IconItem

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile IconItem widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction for Label.
		//		This class should not be used directly.
		//		Mobile IconItem widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_applyAttributes: function(){
			if(!this.textDir && this.getParent() && this.getParent().get("textDir")){
				this.textDir = this.getParent().get("textDir");
			}
			this.inherited(arguments);
		},

		_setLabelAttr: function(text){
			if(this.textDir){
				text = common.enforceTextDirWithUcc(text, this.textDir);
			}
			this.inherited(arguments);
		},

		_setTextDirAttr: function(textDir){
			if(textDir && this.textDir !== textDir){
				this.textDir = textDir;
				this.labelNode.innerHTML = common.removeUCCFromText(this.labelNode.innerHTML);
				this.labelNode.innerHTML = common.enforceTextDirWithUcc(this.labelNode.innerHTML, this.textDir);
				if(this.paneWidget){
					this.paneWidget.labelNode.innerHTML = common.removeUCCFromText(this.paneWidget.labelNode.innerHTML);
					this.paneWidget.labelNode.innerHTML = common.enforceTextDirWithUcc(this.paneWidget.labelNode.innerHTML, this.textDir);		            		            
				}
			}
		}
	});
});
