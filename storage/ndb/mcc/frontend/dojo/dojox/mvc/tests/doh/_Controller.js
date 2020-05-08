define([
	"doh",
	"dojo/_base/declare",
	"dojo/Stateful",
	"dijit/registry",
	"dijit/_WidgetBase",
	"dijit/_TemplatedMixin",
	"dijit/_WidgetsInTemplateMixin",
	"dojox/mvc/at",
	"dojox/mvc/_Controller",
	"dojo/text!../templates/_ControllerInTemplate.html"
], function(doh, declare, Stateful, registry, _WidgetBase, _TemplatedMixin, _WidgetsInTemplateMixin, at, _Controller, template){
	doh.register("dojox.mvc.tests.doh._Controller", [
		function destroyFromWidgetsInTemplate(){
			var w = new (declare([_WidgetBase, _TemplatedMixin, _WidgetsInTemplateMixin], {
				templateString: template
			}))({}, document.createElement("div"));
			w.startup();
			var ctrl = w.controllerNode,
			 id = ctrl.id;
			w.destroy();
			doh.f(registry.byId(id), "The controller should have been removed from registry along with the template widget");
			doh.t(ctrl._destroyed, "The controller should have been marked as destroyed along with the template widget");
		},
		function useWithDijit(){
			var model = new Stateful(),
				w = new (declare([_WidgetBase, _Controller], {}))({
					foo: at(model, "foo")
				});
			w.startup();
			model.set("foo", "Foo");
			doh.is("Foo", w.get("foo"), "Update to model should be reflected to _WidgetBase/Controller mixin");
		}
	]);
});