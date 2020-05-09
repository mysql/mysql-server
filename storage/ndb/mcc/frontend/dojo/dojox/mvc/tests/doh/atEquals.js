define([
	"doh",
	"dojo/Stateful",
	"dijit/_WidgetBase",
	"dojox/mvc/at",
	"dojox/mvc/equals"
], function(doh, Stateful, _WidgetBase, at, equals){
	doh.register("dojox.mvc.tests.doh.atEquals", [
		function single(){
			var a = [0, 1, 2],
			 model = new Stateful({value: a}),
			 w = new _WidgetBase({value: at(model, "value").equals(equals)});
			w.startup();
			model.set("value", a.slice());
			doh.is(a, w.value, "The widget should keep the original value");
			w.set("value", at(model, "value"));
			var copy = a.slice();
			model.set("value", copy);
			doh.is(copy, w.value, "The widget should be updated with the new value");
		},
		function wildcard(){
			var a = [0, 1, 2],
			 model = new Stateful({value: a}),
			 w = new _WidgetBase({_getPropertiesAttr: function(){ return ["value"]; }, "*": at(model, "*").equals(equals)});
			w.startup();
			model.set("value", a.slice());
			doh.is(a, w.value, "The widget should keep the original value");
			w.set("value", at(model, "value"));
			var copy = a.slice();
			model.set("value", copy);
			doh.is(copy, w.value, "The widget should be updated with the new value");
		}
	]);
});
