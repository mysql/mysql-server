define([
	"dojo/_base/declare",
	"dojo/_base/window",
	"dojo/_base/array",
	"dojo/dom-construct",
	"./common"      
], function(declare, win, array, domConstruct, common){

	// module:
	//		dojox/mobile/bidi/SpinWheelSlot

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile SpinWheelSlot widget, using Unicode Control Characters to control text direction.
		// description:
		//		This class should not be used directly.
		//		Mobile SpinWheelSlot widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		postCreate: function(){
			this.inherited(arguments);
			if(!this.textDir && this.getParent() && this.getParent().get("textDir")){
				this.set("textDir", this.getParent().get("textDir"));
			}
		},

		_setTextDirAttr: function(textDir){
			if(textDir && (!this._created || this.textDir !== textDir)){
				this.textDir = textDir;
				this._setTextDirToNodes(this.textDir);
			}
		},

		_setTextDirToNodes: function(textDir){
			array.forEach(this.panelNodes, function(panel){
				array.forEach(panel.childNodes, function(node, i){
					node.innerHTML = common.removeUCCFromText(node.innerHTML);     
					node.innerHTML = common.enforceTextDirWithUcc(node.innerHTML, this.textDir);      
					node.style.textAlign = (this.dir.toLowerCase() === "rtl") ? "right" : "left";      
				}, this);
			}, this);
		}
	});
});
