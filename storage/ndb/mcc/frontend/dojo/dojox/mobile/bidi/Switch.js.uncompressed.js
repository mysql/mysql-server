define("dojox/mobile/bidi/Switch", [
	"dojo/_base/declare",
	"./common",
	"dojo/dom-class"	
], function(declare, common, domClass){

	// module:
	//		dojox/mobile/bidi/Switch

	return declare(null, {
		// summary:
		//		Bidi support for mobile Switch widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction support for LeftLabel and RightLabel.
		//		This class should not be used directly.
		//		Mobile Switch widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		postCreate: function(){
			this.inherited(arguments);
			if(!this.textDir && this.getParent() && this.getParent().get("textDir")){
				this.set("textDir", this.getParent().get("textDir"));
			}
		},
		buildRendering: function(){
			this.inherited(arguments);
			// dojox.mobile mirroring support
			if(!this.isLeftToRight()){
				domClass.add(this.left, "mblSwitchBgLeftRtl");
				domClass.add(this.left.firstChild, "mblSwitchTextLeftRtl");
				domClass.add(this.right, "mblSwitchBgRightRtl");
				domClass.add(this.right.firstChild, "mblSwitchTextRightRtl");
			}
		},
		_newState: function(newState){
			if(this.isLeftToRight()){
				return this.inherited(arguments);
			}
			return (this.inner.offsetLeft < -(this._width/2)) ? "on" : "off";
		},
		_setLeftLabelAttr: function(label){
			this.inherited(arguments);
			this.left.firstChild.innerHTML = common.enforceTextDirWithUcc(this.left.firstChild.innerHTML, this.textDir);
		},

		_setRightLabelAttr: function(label){
			this.inherited(arguments);
			this.right.firstChild.innerHTML = common.enforceTextDirWithUcc(this.right.firstChild.innerHTML, this.textDir);
		},

		_setTextDirAttr: function(textDir){
			if(textDir && (!this._created || this.textDir !== textDir)){
				this.textDir = textDir;
				this.left.firstChild.innerHTML = common.removeUCCFromText(this.left.firstChild.innerHTML);
				this.left.firstChild.innerHTML = common.enforceTextDirWithUcc(this.left.firstChild.innerHTML, this.textDir);
				this.right.firstChild.innerHTML = common.removeUCCFromText(this.right.firstChild.innerHTML);
				this.right.firstChild.innerHTML = common.enforceTextDirWithUcc(this.right.firstChild.innerHTML, this.textDir);
			}
		}
	});
});
