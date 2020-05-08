define([
	"dojo/_base/declare",
	"dojo/dom-style",
	"../_css3"
], function(declare, domStyle, css3){

	// module:
	//		mobile/bidi/Rating

	return declare(null, {

		_setCustomTransform:function(/*Object*/parent){
			domStyle.set(parent, css3.add({"float":"right"}, {transform:"scaleX(-1)"}));
			return parent;
		}
	});
});
