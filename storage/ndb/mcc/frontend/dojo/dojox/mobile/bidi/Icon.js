define([
	"dojo/_base/declare",
	"dojo/dom-style",
	"../_css3"
], function(declare, domStyle, css3){

	// module:
	//		mobile/bidi/Icon

	return declare(null, {

		_setCustomTransform:function(){
			if((this.dir || domStyle.get(this.domNode, "direction")) == "rtl"){
				domStyle.set(this.domNode.firstChild, css3.add({"direction":"ltr"}, {}));
				domStyle.set(this.domNode, css3.add({}, {transform:"scaleX(-1)"}));
			}
		}
	});
});
