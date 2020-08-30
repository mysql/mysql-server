define("dojox/mvc/_atBindingExtension", [
	"dojo/_base/config",
	"dojo/has",
	"dijit/_WidgetBase",
	"./atBindingExtension"
], function(config, has, _WidgetBase, atBindingExtension){
	has.add("mvc-extension-per-widget", (config["mvc"] || {}).extensionPerWidget);
	if(!has("mvc-extension-per-widget")){
		atBindingExtension(_WidgetBase.prototype);
	}
});
