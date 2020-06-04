define("dojox/mobile/bidi/Badge", ["dojo/_base/declare", "./common"], function(declare, common){

	// module:
	//		dojox/mobile/bidi/Badge

	return declare(null, {
		// summary:
		//		Support for control over text direction for Badge, using Unicode Control Characters to control text direction.
		// description:
		//		Added textDir attribute, similar to mobile widgets based on dijit._WidgetBase.
		//		Extension to value setting attributes, with text direction support.
		//		This class should not be used directly.
		//		Mobile Badge widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		
		// textDir: String
		//		Mobile widgets, derived from dijit._WidgetBase has this attribute for text direction support (bidi support).
		//		The text direction can be different than the GUI direction.
		//		Values: "ltr", "rtl", "auto"(the direction of a text defined by first strong letter).
		textDir: "", 

		setValue: function(/*String*/value){
			this.domNode.firstChild.innerHTML = common.enforceTextDirWithUcc(value, this.textDir);
		},

		setTextDir: function(/*String*/textDir){
			if (this.textDir !== textDir){
				this.textDir = textDir;
				this.domNode.firstChild.innerHTML = common.enforceTextDirWithUcc(common.removeUCCFromText(this.domNode.firstChild.innerHTML), this.textDir);
			}
		}
	});
});
