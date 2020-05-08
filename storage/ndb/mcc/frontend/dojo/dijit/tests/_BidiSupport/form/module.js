define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	var test_robot = has("trident") || has("ff") || has("chrome") < 45;

	doh.register("_BidiSupport.form.test_PlaceholderInput.", require.toUrl("./test_PlaceholderInput.html"), 999999);

	doh.register("_BidiSupport.form.multiSelect", require.toUrl("./multiSelect.html"), 999999);

	doh.register("_BidiSupport.form.noTextDirTextWidgets", require.toUrl("./noTextDirTextWidgets.html"), 999999);

	doh.register("_BidiSupport.form.Button", require.toUrl("./Button.html"), 999999);

	doh.register("_BidiSupport.form.RadioButton", require.toUrl("./RadioButton.html"), 999999);

	doh.register("_BidiSupport.form.Select", require.toUrl("./test_Select.html"), 999999);

	doh.register("_BidiSupport.form.Slider", require.toUrl("./test_Slider.html"), 999999);

	if(test_robot){
		doh.register("_BidiSupport.form.robot.Textarea", require.toUrl("./robot/Textarea.html"), 999999);

		doh.register("_BidiSupport.form.robot.SimpleComboBoxes", require.toUrl("./robot/SimpleComboBoxes.html"), 999999);

		doh.register("_BidiSupport.form.robot.SimpleTextarea", require.toUrl("./robot/SimpleTextarea.html"), 999999);

		doh.register("_BidiSupport.form.robot.TextBoxes", require.toUrl("./robot/TextBoxes.html"), 999999);

		doh.register("_BidiSupport.form.robot.InlineEditBox", require.toUrl("./robot/InlineEditBox.html"), 999999);
	}

	doh.register("_BidiSupport.form.TimeTextBox", require.toUrl("./test_TimeTextBox.html?mode=test"), 999999);

});