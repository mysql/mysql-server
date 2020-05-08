define([
	"dojo/_base/declare",
	"dijit/_BidiSupport"  //load implementation for textDir from dijit (for editable widgets), (no direct references)
], function(declare){

	// module:
	//		dojox/mobile/bidi/TextBox

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile TextBox widget.
		// description:
		//		Implementation for text direction using HTML "dir" attribute (used for editable widgets).
		//		This class should not be used directly.
		//		Mobile TextBox widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setTextDirAttr: function(/*String*/ textDir){
			if(!this._created || this.textDir != textDir){
				this._set("textDir", textDir);
				if(this.value){
					this.applyTextDir(this.focusNode || this.textbox);
				}
				else{
					this.applyTextDir(this.focusNode || this.textbox, this.textbox.getAttribute("placeholder"));
				}
			}
		},

		_setDirAttr: function(/*String*/ dir){
			if(!(this.textDir && this.textbox)){
				this.dir = dir;
			}
		},

		_onBlur: function(e){
			this.inherited(arguments);
			if(!this.textbox.value){
				this.applyTextDir(this.textbox, this.textbox.getAttribute("placeholder"));
			}
		},

		_onInput: function(e){
			this.inherited(arguments);
			if(!this.textbox.value){
				this.applyTextDir(this.textbox, this.textbox.getAttribute("placeholder"));
			}
		}
	});
});
