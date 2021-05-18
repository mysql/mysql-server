define("dojox/mobile/CheckBox", [
	"dojo/_base/declare",
	"dojo/dom-construct",
	"dijit/form/_CheckBoxMixin",
	"./ToggleButton",
	"./sniff"
],
	function(declare, domConstruct, CheckBoxMixin, ToggleButton, has){

	return declare("dojox.mobile.CheckBox", [ToggleButton, CheckBoxMixin], {
		// summary:
		//		A non-templated checkbox widget that can be in two states 
		//		(checked or not checked).

		// baseClass: String
		//		The name of the CSS class of this widget.
		baseClass: "mblCheckBox",

		// _setTypeAttr: [private] Function 
		//		Overrides the automatic assignement of type to nodes.
		_setTypeAttr: function(){}, // cannot be changed: IE complains w/o this

		buildRendering: function(){
			if(!this.templateString && !this.srcNodeRef){
				// The following doesn't work on IE < 8 if the default state is checked.
				// You have to use "<input checked>" instead but it's not worth the bytes here.
				this.srcNodeRef = domConstruct.create("input", {type: this.type});
			}
			this.inherited(arguments);
			if(!this.templateString){
				// if this widget is templated, let the template set the focusNode via an attach point
				this.focusNode = this.domNode;
			}

			if(has("windows-theme")){
				var rootNode = domConstruct.create("span", {className: "mblCheckableInputContainer"});
				rootNode.appendChild(this.domNode.cloneNode());
				this.labelNode = domConstruct.create("span", {className: "mblCheckableInputDecorator"}, rootNode);
				this.domNode = rootNode;
				this.focusNode = rootNode.firstChild;
			}
		},
		
		_getValueAttr: function(){
			// tags:
			//		private
			return (this.checked ? this.value : false);
		}
	});
});
