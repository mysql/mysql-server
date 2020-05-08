define([
	"dojo/_base/declare",
	"./common",
	"dojo/dom-style"
], function(declare,common, domStyle){

	// module:
	//		dojox/mobile/bidi/Carousel

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile Carousel widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction support for Title.
		//		This class should not be used directly.
		//		Mobile Carousel widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		
		buildRendering: function(){
			this.inherited(arguments);
			// dojox.mobile mirroring support
		 	if(!this.isLeftToRight()){
				if(this.navButton){
					domStyle.set(this.btnContainerNode, "float", "left"); // workaround for webkit rendering problem
					this.disconnect(this._prevHandle);
					this.disconnect(this._nextHandle);
					this._prevHandle = this.connect(this.prevBtnNode, "onclick", "onNextBtnClick");
					this._nextHandle = this.connect(this.nextBtnNode, "onclick", "onPrevBtnClick");
				}
				
				if(this.pageIndicator){
					domStyle.set(this.piw.domNode, "float", "left"); // workaround for webkit rendering problem
				}
			} 
		},
		_setTitleAttr: function(title){
			this.titleNode.innerHTML = this._cv ? this._cv(title) : title;
			this._set("title", title);
			if(this.textDir){
				this.titleNode.innerHTML = common.enforceTextDirWithUcc(this.titleNode.innerHTML, this.textDir);
				this.titleNode.style.textAlign = (this.dir.toLowerCase() === "rtl") ? "right" : "left";
			}
		},
		_setTextDirAttr: function(textDir){
			if(textDir && this.textDir !== textDir){
				this.textDir = textDir;
				this.titleNode.innerHTML = common.removeUCCFromText(this.titleNode.innerHTML);
				this.titleNode.innerHTML = common.enforceTextDirWithUcc(this.titleNode.innerHTML, this.textDir);
				if(this.items.length > 0)
					this.onComplete(this.items);
				} 
		}
	});
});
