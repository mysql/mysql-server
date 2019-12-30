require({cache:{
'url:dijit/form/templates/DropDownButton.html':"<span class=\"dijit dijitReset dijitInline\"\n\t><span class='dijitReset dijitInline dijitButtonNode'\n\t\tdata-dojo-attach-event=\"ondijitclick:_onClick\" data-dojo-attach-point=\"_buttonNode\"\n\t\t><span class=\"dijitReset dijitStretch dijitButtonContents\"\n\t\t\tdata-dojo-attach-point=\"focusNode,titleNode,_arrowWrapperNode,_popupStateNode\"\n\t\t\trole=\"button\" aria-haspopup=\"true\" aria-labelledby=\"${id}_label\"\n\t\t\t><span class=\"dijitReset dijitInline dijitIcon\"\n\t\t\t\tdata-dojo-attach-point=\"iconNode\"\n\t\t\t></span\n\t\t\t><span class=\"dijitReset dijitInline dijitButtonText\"\n\t\t\t\tdata-dojo-attach-point=\"containerNode\"\n\t\t\t\tid=\"${id}_label\"\n\t\t\t></span\n\t\t\t><span class=\"dijitReset dijitInline dijitArrowButtonInner\"></span\n\t\t\t><span class=\"dijitReset dijitInline dijitArrowButtonChar\">&#9660;</span\n\t\t></span\n\t></span\n\t><input ${!nameAttrSetting} type=\"${type}\" value=\"${value}\" class=\"dijitOffScreen\" tabIndex=\"-1\"\n\t\tdata-dojo-attach-point=\"valueNode\" aria-hidden=\"true\"\n/></span>\n"}});
define("dijit/form/DropDownButton", [
	"dojo/_base/declare", // declare
	"dojo/_base/lang",	// hitch
	"dojo/query", // query
	"../registry",	// registry.byNode
	"../popup",		// dijit.popup2.hide
	"./Button",
	"../_Container",
	"../_HasDropDown",
	"dojo/text!./templates/DropDownButton.html"
], function(declare, lang, query, registry, popup, Button, _Container, _HasDropDown, template){

// module:
//		dijit/form/DropDownButton


return declare("dijit.form.DropDownButton", [Button, _Container, _HasDropDown], {
	// summary:
	//		A button with a drop down
	//
	// example:
	// |	<button data-dojo-type="dijit/form/DropDownButton">
	// |		Hello world
	// |		<div data-dojo-type="dijit/Menu">...</div>
	// |	</button>
	//
	// example:
	// |	var button1 = new DropDownButton({ label: "hi", dropDown: new dijit.Menu(...) });
	// |	win.body().appendChild(button1);
	//

	baseClass : "dijitDropDownButton",

	templateString: template,

	_fillContent: function(){
		// Overrides Button._fillContent().
		//
		// My inner HTML contains both the button contents and a drop down widget, like
		// <DropDownButton>  <span>push me</span>  <Menu> ... </Menu> </DropDownButton>
		// The first node is assumed to be the button content. The widget is the popup.

		if(this.srcNodeRef){ // programatically created buttons might not define srcNodeRef
			//FIXME: figure out how to filter out the widget and use all remaining nodes as button
			//	content, not just nodes[0]
			var nodes = query("*", this.srcNodeRef);
			this.inherited(arguments, [nodes[0]]);

			// save pointer to srcNode so we can grab the drop down widget after it's instantiated
			this.dropDownContainer = this.srcNodeRef;
		}
	},

	startup: function(){
		if(this._started){ return; }

		// the child widget from srcNodeRef is the dropdown widget.  Insert it in the page DOM,
		// make it invisible, and store a reference to pass to the popup code.
		if(!this.dropDown && this.dropDownContainer){
			var dropDownNode = query("[widgetId]", this.dropDownContainer)[0];
			this.dropDown = registry.byNode(dropDownNode);
			delete this.dropDownContainer;
		}
		if(this.dropDown){
			popup.hide(this.dropDown);
		}

		this.inherited(arguments);
	},

	isLoaded: function(){
		// Returns whether or not we are loaded - if our dropdown has an href,
		// then we want to check that.
		var dropDown = this.dropDown;
		return (!!dropDown && (!dropDown.href || dropDown.isLoaded));
	},

	loadDropDown: function(/*Function*/ callback){
		// Default implementation assumes that drop down already exists,
		// but hasn't loaded it's data (ex: ContentPane w/href).
		// App must override if the drop down is lazy-created.
		var dropDown = this.dropDown;
		var handler = dropDown.on("load", lang.hitch(this, function(){
			handler.remove();
			callback();
		}));
		dropDown.refresh();		// tell it to load
	},

	isFocusable: function(){
		// Overridden so that focus is handled by the _HasDropDown mixin, not by
		// the _FormWidget mixin.
		return this.inherited(arguments) && !this._mouseDown;
	}
});

});
