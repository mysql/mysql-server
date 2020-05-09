define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	var test_robot = has("trident") || has("ff") || has("chrome") < 45;

	// Utility methods (previously in dijit/_base)
	doh.register("registry", require.toUrl("./registry.html"), 999999);
	doh.register("focus", require.toUrl("./focus.html"), 999999);
	doh.register("place", require.toUrl("./place.html"), 999999);
	doh.register("place-margin", require.toUrl("./place-margin.html"), 999999);
	doh.register("place-clip", require.toUrl("./place-clip.html"), 999999);
	doh.register("popup", require.toUrl("./popup.html"), 999999);
	doh.register("a11y", require.toUrl("./a11y.html"), 999999);
	if(test_robot){
		doh.register("robot.typematic", require.toUrl("./robot/typematic.html"), 999999);
	}

	// _Widget
	doh.register("_Widget-lifecycle", require.toUrl("./_Widget-lifecycle.html"), 999999);
	doh.register("_Widget-attr", require.toUrl("./_Widget-attr.html"), 999999);
	doh.register("_Widget-subscribe", require.toUrl("./_Widget-subscribe.html"), 999999);
	doh.register("_Widget-placeAt", require.toUrl("./_Widget-placeAt.html"), 999999);
	doh.register("_Widget-on", require.toUrl("./_Widget-on.html"), 999999);
	if(test_robot){
		doh.register("robot._Widget-deferredConnect", require.toUrl("./robot/_Widget-deferredConnect.html"), 999999);
		doh.register("robot._Widget-ondijitclick_mouse", require.toUrl("./robot/_Widget-ondijitclick_mouse.html"), 999999);
		doh.register("robot._Widget-ondijitclick_a11y", require.toUrl("./robot/_Widget-ondijitclick_a11y.html"), 999999);
	}

	// _Templated and other mixins
	doh.register("_AttachMixin", require.toUrl("./_AttachMixin.html"), 999999);
	doh.register("_TemplatedMixin", require.toUrl("./_TemplatedMixin.html"), 999999);
	doh.register("_WidgetsInTemplateMixin", require.toUrl("./_WidgetsInTemplateMixin.html"), 999999);
	doh.register("_Templated-widgetsInTemplate1.x", require.toUrl("./_Templated-widgetsInTemplate1.x.html"), 999999);
	doh.register("_Container", require.toUrl("./_Container.html"), 999999);
	doh.register("_KeyNavContainer", require.toUrl("./_KeyNavContainer.html"), 999999);
	if(test_robot){
		doh.register("robot._KeyNavContainer", require.toUrl("./robot/_KeyNavContainer.html"), 999999);
	}
	doh.register("_HasDropDown", require.toUrl("./_HasDropDown.html"), 999999);

	doh.register("Declaration", require.toUrl("./test_Declaration.html"), 999999);
	doh.register("Declaration_1.x", require.toUrl("./test_Declaration_1.x.html"), 999999);

	// Miscellaneous
	doh.register("NodeList-instantiate", require.toUrl("./NodeList-instantiate.html"), 999999);
	doh.register("Destroyable", require.toUrl("./Destroyable.html"), 999999);
	if(test_robot){
		doh.register("robot.BgIframe", require.toUrl("./robot/BgIframe.html"), 999999);
	}

});
