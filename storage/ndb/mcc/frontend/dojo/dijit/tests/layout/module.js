define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	var test_robot = has("trident") || has("ff") || has("chrome") < 45;

	doh.register("layout.ContentPane", require.toUrl("./ContentPane.html"), 999999);
	doh.register("layout.test_ContentPane", require.toUrl("./test_ContentPane.html"), 999999);
	doh.register("layout.ContentPaneLayout", require.toUrl("./ContentPaneLayout.html"), 999999);
	doh.register("layout.ContentPane-remote", require.toUrl("./ContentPane-remote.html"), 999999);
	doh.register("layout.ContentPane-auto-require", require.toUrl("./ContentPane-auto-require.html"), 999999);

	if(test_robot){
		doh.register("layout.robot.GUI", require.toUrl("./robot/GUI.html"), 999999);
	}

	doh.register("layout.LayoutContainer_v1", require.toUrl("./LayoutContainer_v1.html"), 999999);
	doh.register("layout.LayoutContainer", require.toUrl("./LayoutContainer.html"), 999999);

	doh.register("layout.StackContainer", require.toUrl("./StackContainer.html"), 999999);
	doh.register("layout.NestedStackContainer", require.toUrl("./nestedStack.html"), 999999);

	doh.register("layout.TabContainer", require.toUrl("./TabContainer.html"), 999999);
	if(test_robot){
		doh.register("layout.robot.TabContainer_a11y", require.toUrl("./robot/TabContainer_a11y.html"), 999999);
		doh.register("layout.robot.TabContainer_mouse", require.toUrl("./robot/TabContainer_mouse.html"), 999999);
		doh.register("layout.robot.TabContainer_noLayout", require.toUrl("./robot/TabContainer_noLayout.html"), 999999);
	}
	doh.register("layout.TabContainerTitlePane", require.toUrl("./TabContainerTitlePane.html"), 999999);
	
	doh.register("layout.AccordionContainer", require.toUrl("./AccordionContainer.html"), 999999);
	if(test_robot){
		doh.register("layout.robot.AccordionContainer_a11y", require.toUrl("./robot/AccordionContainer_a11y.html"), 999999);
		doh.register("layout.robot.AccordionContainer_mouse", require.toUrl("./robot/AccordionContainer_mouse.html"), 999999);
	}

	doh.register("layout.BorderContainer", require.toUrl("./BorderContainer.html"), 999999);
	if(test_robot){
		doh.register("layout.robot.BorderContainer", require.toUrl("./robot/BorderContainer.html"), 999999);
		doh.register("layout.robot.BorderContainer_full", require.toUrl("./robot/BorderContainer_full.html"), 999999);
		doh.register("layout.robot.BorderContainer_complex", require.toUrl("./robot/BorderContainer_complex.html"), 999999);
		doh.register("layout.robot.BorderContainer_nested", require.toUrl("./robot/BorderContainer_nested.html"), 999999);
	}

});
