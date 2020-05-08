define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	var test_robot = has("trident") || has("ff") || has("chrome") < 45;

	doh.register("tree.CustomLabel", require.toUrl("./CustomLabel.html"), 999999);
	doh.register("tree.Tree_ForestStoreModel", require.toUrl("./Tree_ForestStoreModel.html"), 999999);
	doh.register("tree.Tree_with_JRS", require.toUrl("./Tree_with_JRS.html"), 999999);
	doh.register("tree.Tree_ObjectStoreModel", require.toUrl("./Tree_ObjectStoreModel.html"), 999999);

	if(test_robot){
		doh.register("tree.robot.Tree_a11y", require.toUrl("./robot/Tree_a11y.html"), 999999);
		doh.register("tree.robot.Tree_mouse", require.toUrl("./robot/Tree_mouse.html"), 999999);
		doh.register("tree.robot.Tree_Custom_TreeNode", require.toUrl("./robot/Tree_Custom_TreeNode.html"), 999999);
		doh.register("tree.robot.Tree_DnD", require.toUrl("./robot/Tree_dnd.html"), 999999);
		doh.register("tree.robot.Tree_selector", require.toUrl("./robot/Tree_selector.html"), 999999);
		doh.register("tree.robot.Tree_selector_only",
			require.toUrl("./robot/Tree_selector.html?controller=selector"), 999999);
		doh.register("tree/robot.Tree_DnD_multiParent", require.toUrl("./robot/Tree_dnd_multiParent.html"), 999999);
		doh.register("tree.robot.Tree_v1", require.toUrl("./robot/Tree_v1.html"), 999999);
	}

});
