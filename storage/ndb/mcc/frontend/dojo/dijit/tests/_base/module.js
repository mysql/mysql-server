define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	var test_robot = has("trident") || has("ff") || has("chrome") < 45;

	doh.register("_base.manager", require.toUrl("./manager.html"), 999999);
	doh.register("_base.wai", require.toUrl("./wai.html"), 999999);
	doh.register("_base.place", require.toUrl("./place.html"), 999999);
	doh.register("_base.popup", require.toUrl("./popup.html"), 999999);
	if(test_robot){
		doh.register("_base.robot.CrossWindow", require.toUrl("./robot/CrossWindow.html"), 999999);
		doh.register("_base.robot.FocusManager", require.toUrl("./robot/FocusManager.html"), 999999);
		doh.register("_base.robot.focus_mouse", require.toUrl("./robot/focus_mouse.html"), 999999);
	}

});
