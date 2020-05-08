define(["dojo/_base/lang", "dojo/dom", "dojo/dom-style", "dijit/registry", "dojox/mobile/TransitionEvent", "dojo/sniff"],
	function(lang, dom, domStyle, registry, TransitionEvent, has){
	return {
		init: function(){
			if(!has("phone")){
				domStyle.set(dom.byId("gotoConfigurationView"), "display", "none");
			}
/*
			registry.byId("itemslist_add").on("click", lang.hitch(this, function(e){
				// use selected_item = -1 to identify add a new item
				this.app._addNewItem = true;

				// transition to detail view for edit
				var transOpts = {
					title: "Detail",
					target: "details,EditTodoItem",
					url: "#details,EditTodoItem"
				};
				new TransitionEvent(e.target, transOpts, e).dispatch();
			}));
*/			
		}
	}
});
