define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	var test_robot = has("trident") || has("ff") || has("chrome") < 45;

	// inline doh tests
	doh.register("editor.Editor", require.toUrl("./Editor.html"), 999999);
	doh.register("editor.nls_8859-2", require.toUrl("./nls_8859-2.html"), 999999);
	doh.register("editor.nls_sjis", require.toUrl("./nls_sjis.html"), 999999);
	doh.register("editor.nls_utf8", require.toUrl("./nls_utf8.html"), 999999);
	doh.register("editor.Editor_stylesheet", require.toUrl("./Editor_stylesheet.html"), 999999);
	doh.register("editor.html", require.toUrl("./html.html"), 999999);

	// Base editor functionality
	if(test_robot){
		doh.register("editor.robot.Editor_mouse", require.toUrl("./robot/Editor_mouse.html"), 999999);
		doh.register("editor.robot.Editor_a11y", require.toUrl("./robot/Editor_a11y.html"), 999999);
		doh.register("editor.robot.Misc", require.toUrl("./robot/Editor_misc.html"), 999999);
		doh.register("editor.robot.Bidi", require.toUrl("./robot/Editor_bidi.html"), 999999);
	}

	// Plugins
	if(test_robot){
		doh.register("editor.robot.CustomPlugin", require.toUrl("./robot/CustomPlugin.html"), 999999);
		doh.register("editor.robot.EnterKeyHandling", require.toUrl("./robot/EnterKeyHandling.html"), 999999);
		doh.register("editor.robot.FullScreen", require.toUrl("./robot/Editor_FullScreen.html"), 999999);
		doh.register("editor.robot.ViewSource", require.toUrl("./robot/Editor_ViewSource.html"), 999999);
		doh.register("editor.robot.NewPage", require.toUrl("./robot/Editor_NewPage.html"), 999999);
		doh.register("editor.robot.LinkDialog", require.toUrl("./robot/Editor_LinkDialog.html"), 999999);
		doh.register("editor.robot.FontChoice", require.toUrl("./robot/Editor_FontChoice.html"), 999999);
		doh.register("editor.robot.ToggleDir", require.toUrl("./robot/ToggleDir.html"), 999999);
		doh.register("editor.robot.TabIndent", require.toUrl("./robot/TabIndent.html"), 999999);
	}

	if(test_robot && !has("webkit")){
		// The back button on webkit is URL for the browser itself, restarting the entire test suite,
		// rather than just for the iframe holding the test file (BackForwardState.html and BackForwardStateHelper.html)
		doh.register("editor.robot.BackForwardState", require.toUrl("./robot/BackForwardState.html"), 999999);
	}

	// Special test for IE9 in IE8 compat mode (#14900)
	if(test_robot && has("ie") == 9){
		doh.register("editor.robot.Editor_IE8Compat", require.toUrl("./robot/Editor_IE8Compat.html"), 999999);
	}

});



