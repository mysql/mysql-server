define([
	"dojo/_base/declare",
	"./common"   
], function(declare, common){

	// module:
	//		dojox/mobile/bidi/ValuePickerSlot

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile ValuePickerSlot widget, using Unicode Chontrol Characters to control text direction.
		// description:
		//		Implementation for text direction support for Value.
		//		This class should not be used directly.
		//		Mobile ValuePickerSlot widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		postCreate: function(){
			if(!this.textDir && this.getParent() && this.getParent().get("textDir")){
				this.textDir = this.getParent().get("textDir");
			}
		},

		_getValueAttr: function(){
			return common.removeUCCFromText(this.inputNode.value);
		},

		_setValueAttr: function(value){
			this.inherited(arguments);
			this._applyTextDirToValueNode();
		},

		_setTextDirAttr: function(textDir){
			if(textDir && this.textDir !== textDir){
				this.textDir = textDir;
				this._applyTextDirToValueNode();
			}
		},

		_applyTextDirToValueNode: function(){
			this.inputNode.value = common.removeUCCFromText(this.inputNode.value);
			this.inputNode.value = common.enforceTextDirWithUcc(this.inputNode.value, this.textDir);
		}
	});
});
