define(["dojo/_base/declare", "dojox/app/ViewBase", "dijit/form/Button"],
	function(declare, ViewBase, Button){
		return declare([Button, ViewBase], {
			postscript: function(){
				// we want to avoid kickin the Dijit lifecycle at ctor time so that definition has been mixed into the
				// widget when it is instanciated. This is only really needed if you need the definition
			},
			_startup: function(){
				this.create();
				this.inherited(arguments);
			}
		});
	}
);