define("dojox/mobile/bidi/CarouselItem", [
	"dojo/_base/declare",
	"./common"
], function(declare, common){

	// module:
	//		dojox/mobile/bidi/CarouselItem

	return declare(null, {
		// summary:
		//		Support for control over text direction for mobile CarouselItem widget, using Unicode Control Characters to control text direction.
		// description:
		//		Implementation for text direction support for Header and Footer.
		//		This class should not be used directly.
		//		Mobile CarouselItem widget loads this module when user sets "has: {'dojo-bidi': true }" in data-dojo-config.
		_setHeaderTextAttr: function(text){
			this._set("headerText", text);
			this.headerTextNode.innerHTML = this._cv ? this._cv(text) : text;
			var p = this.getParent() ? this.getParent().getParent() : null;
			this.textDir = this.textDir ? this.textDir : p ? p.get("textDir") : ""; //take textDir from Carousel
			if(this.textDir){
				this.headerTextNode.innerHTML = common.enforceTextDirWithUcc(this.headerTextNode.innerHTML, this.textDir);
			}
		},

		_setFooterTextAttr: function(text){
			this._set("footerText", text);
			this.footerTextNode.innerHTML = this._cv ? this._cv(text) : text;
			var p = this.getParent() ? this.getParent().getParent() : null;
			this.textDir = this.textDir ? this.textDir : p ? p.get("textDir") : ""; //take textDir from Carousel
			if(this.textDir){
				this.footerTextNode.innerHTML = _BidiSupport.enforceTextDirWithUcc(this.footerTextNode.innerHTML, this.textDir);
			}
		}
	});
});
