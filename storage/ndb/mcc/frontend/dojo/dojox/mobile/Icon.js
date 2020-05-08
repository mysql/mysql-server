define([
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dojo/dom-class",
	"dojo/dom-construct",
	"./iconUtils",
	"dojo/has",
	"dojo/has!dojo-bidi?dojox/mobile/bidi/Icon"
], function(declare, lang, domClass, domConstruct, iconUtils, has, BidiIcon){

	// module:
	//		dojox/mobile/Icon

	var Icon = declare(has("dojo-bidi") ? "dojox.mobile.NonBidiIcon" : "dojox.mobile.Icon", null, {
		// summary:
		//		A wrapper for image icon, CSS sprite icon, or DOM Button.
		// description:
		//		Icon is a simple utility class for creating an image icon, a CSS sprite icon, 
		//		or a DOM Button. It calls dojox/mobile/iconUtils.createIcon() with the 
		//		appropriate parameters to create an icon. 
		//		Note that this module is not a widget, that is it does not inherit 
		//		from dijit/_WidgetBase.
		// example:
		//		Image icon:
		//	|	<div data-dojo-type="dojox.mobile.Icon"
		//	|		data-dojo-props='icon:"images/tab-icon-12h.png"'></div>
		//
		//		CSS sprite icon:
		//	|	<div data-dojo-type="dojox.mobile.Icon"
		//	|		data-dojo-props='icon:"images/tab-icons.png",iconPos:"29,116,29,29"'></div>
		//
		//		DOM Button:
		//	|	<div data-dojo-type="dojox.mobile.Icon"
		//	|		data-dojo-props='icon:"mblDomButtonBlueCircleArrow"'></div>

		// icon: [const] String
		//		An icon to display. The value can be either a path for an image
		//		file or a class name of a DOM button.
		//		Note that changing the value of the property after the icon
		//		creation has no effect.
		icon: "",

		// iconPos: [const] String
		//		The position of an aggregated icon. IconPos is comma separated
		//		values like top,left,width,height (ex. "0,0,29,29").
		//		Note that changing the value of the property after the icon
		//		creation has no effect.
		iconPos: "",

		// alt: [const] String
		//		An alt text for the icon image.
		//		Note that changing the value of the property after the icon
		//		creation has no effect.
		alt: "",

		// tag: String
		//		The name of the HTML tag to create as this.domNode.
		tag: "div",

		constructor: function(/*Object?*/args, /*DomNode?*/node){
			// summary:
			//		Creates a new instance of the class.
			// args:
			//		Contains properties to be set.
			// node:
			//		The DOM node. If none is specified, it is automatically created. 
			if(args){
				lang.mixin(this, args);
			}
			this.domNode = node || domConstruct.create(this.tag);
			iconUtils.createIcon(this.icon, this.iconPos, null, this.alt, this.domNode);
			this._setCustomTransform();
		},
		_setCustomTransform: function(){
			// summary:
			//		To be implemented in bidi/Icon.js.
		}
	});
	return has("dojo-bidi") ? declare("dojox.mobile.Icon", [Icon, BidiIcon]) : Icon;
});
