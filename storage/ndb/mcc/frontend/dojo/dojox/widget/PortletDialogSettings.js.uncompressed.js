define("dojox/widget/PortletDialogSettings", [
	"dojo/_base/declare",
	"dojo/_base/window",
	"dojo/dom-style",
	"./PortletSettings",
	"dijit/Dialog"
	], function(declare, window, domStyle, PortletSettings, Dialog){
		
	return declare("dojox.widget.PortletDialogSettings", [PortletSettings],{
			// summary:
			//		A settings widget to be used with a dojox.widget.Portlet, which displays
			//		the contents of this widget in a dijit.Dialog box.

			// dimensions: Array
			//		The size of the dialog to display.	This defaults to [300, 300]
			dimensions: null,

			constructor: function(props, node){
				this.dimensions = props.dimensions || [300, 100];
			},

			toggle: function(){
				// summary:
				//		Shows and hides the Dialog box.
				
				if(!this.dialog){
					//require("dijit.Dialog");
					this.dialog = new Dialog({title: this.title});

					window.body().appendChild(this.dialog.domNode);

					// Move this widget inside the dialog
					this.dialog.containerNode.appendChild(this.domNode);

					domStyle.set(this.dialog.domNode,{
						"width" : this.dimensions[0] + "px",
						"height" : this.dimensions[1] + "px"
					});
					domStyle.set(this.domNode, "display", "");
				}
				if(this.dialog.open){
					this.dialog.hide();
				}else{
					this.dialog.show(this.domNode);
				}
			}
	});
});
