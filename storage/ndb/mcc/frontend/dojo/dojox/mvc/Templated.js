define([
	"dojo/_base/declare",
	"dojo/_base/lang",
	"dijit/_WidgetBase",
	"dijit/_TemplatedMixin",
	"dijit/_WidgetsInTemplateMixin",
	"./at"
], function(declare, lang, _WidgetBase, _TemplatedMixin, _WidgetsInTemplateMixin){
	return declare("dojox.mvc.Templated", [_WidgetBase, _TemplatedMixin, _WidgetsInTemplateMixin], {
		// summary:
		//		A templated widget, mostly the same as dijit/_Templated, but without deprecated features in it.

		// bindings: Object|Function
		//		The data binding declaration (or simple parameters) for child widgets.
		bindings: null,

		startup: function(){
			// Code to support childBindings property in dojox/mvc/WidgetList, etc.
			// This implementation makes sure childBindings is set before this widget starts up, as dijit/_WidgetsInTemplatedMixin starts up child widgets before it starts itself up.
			var bindings = lang.isFunction(this.bindings) && this.bindings.call(this) || this.bindings;
			for(var s in bindings){
				var w = this[s], props = bindings[s];
				if(w){
					for(var prop in props){
						w.set(prop, props[prop]);
					}
				}else{
					console.warn("Widget with the following attach point was not found: " + s);
				}
			}
			this.inherited(arguments);
		}
	});
});
