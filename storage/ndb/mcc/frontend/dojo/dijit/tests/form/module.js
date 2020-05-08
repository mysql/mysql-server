define(["doh/main", "require", "dojo/sniff"], function(doh, require, has){

	var test_robot = has("trident") || has("ff") || has("chrome") < 45;

	doh.register("form.ButtonMixin", require.toUrl("./ButtonMixin.html"), 999999);
	doh.register("form.Button", require.toUrl("./Button.html"), 999999);
	doh.register("form.ToggleButtonMixin", require.toUrl("./ToggleButtonMixin.html"), 999999);
	if(test_robot){
		doh.register("form.robot.Button_mouse", require.toUrl("./robot/Button_mouse.html"), 999999);
		doh.register("form.robot.Button_a11y", require.toUrl("./robot/Button_a11y.html"), 999999);
	}

	doh.register("form.CheckBoxMixin", require.toUrl("./CheckBoxMixin.html"), 999999);
	doh.register("form.CheckBox", require.toUrl("./CheckBox.html"), 999999);
	doh.register("form.RadioButtonMixin", require.toUrl("./RadioButtonMixin.html"), 999999);
	doh.register("form.RadioButtonTiming", require.toUrl("./RadioButtonTiming.html?mode=test"), 999999);
	if(test_robot){
		doh.register("form.robot.CheckBox_mouse", require.toUrl("./robot/CheckBox_mouse.html"), 999999);
		doh.register("form.robot.CheckBox_a11y", require.toUrl("./robot/CheckBox_a11y.html"), 999999);
	}

	doh.register("form.test_validate", require.toUrl("./test_validate.html?mode=test"), 999999);
	if(test_robot){
		doh.register("form.robot.ValidationTextBox", require.toUrl("./robot/ValidationTextBox.html"), 999999);
		doh.register("form.robot.TextBox_onInput", require.toUrl("./robot/TextBox_onInput.html"), 999999);
	}

	doh.register("form.DateTextBox", require.toUrl("./test_DateTextBox.html?mode=test"), 999999);
	if(test_robot){
		doh.register("form.robot.DateTextBox", require.toUrl("./robot/DateTextBox.html"), 999999);
		doh.register("form.robot.TimeTextBox", require.toUrl("./robot/TimeTextBox.html"), 999999);
	}

	doh.register("form.Form", require.toUrl("./Form.html"), 999999);
	if(test_robot){
		doh.register("form.robot.FormState", require.toUrl("./robot/Form_state.html"), 999999);
		doh.register("form.robot.Form_onsubmit", require.toUrl("./robot/Form_onsubmit.html"), 999999);
	}

	doh.register("form.Select", require.toUrl("./test_Select.html?mode=test"), 999999);
	if(test_robot){
		doh.register("form.robot.Select", require.toUrl("./robot/Select.html"), 999999);
	}

	doh.register("form.AutoCompleterMixin", require.toUrl("./AutoCompleterMixin.html"), 999999);
	doh.register("form.ComboBox", require.toUrl("./_autoComplete.html?testWidget=dijit.form.ComboBox&mode=test"), 999999);
	if(test_robot){
		doh.register("form.robot.ComboBox_mouse", require.toUrl("./robot/_autoComplete_mouse.html?testWidget=dijit.form.ComboBox"), 999999);
		doh.register("form.robot.ComboBox_a11y", require.toUrl("./robot/_autoComplete_a11y.html?testWidget=dijit.form.ComboBox"), 999999);
	}
	doh.register("form.FilteringSelect", require.toUrl("./_autoComplete.html?testWidget=dijit.form.FilteringSelect&mode=test"), 999999);
	if(test_robot){
		doh.register("form.robot.FilteringSelect_mouse", require.toUrl("./robot/_autoComplete_mouse.html?testWidget=dijit.form.FilteringSelect"), 999999);
		doh.register("form.robot.FilteringSelect_a11y", require.toUrl("./robot/_autoComplete_a11y.html?testWidget=dijit.form.FilteringSelect"), 999999);
	}

	doh.register("form.MultiSelect", require.toUrl("./test_MultiSelect.html?mode=test"), 999999);
	if(test_robot){
		doh.register("form.robot.MultiSelect", require.toUrl("./robot/MultiSelect.html"), 999999);
	}

	if(test_robot){
		doh.register("form.robot.SimpleTextarea", require.toUrl("./robot/SimpleTextarea.html"), 999999);
	}

	doh.register("form.Slider", require.toUrl("./Slider.html"), 999999);
	if(test_robot){
		doh.register("form.robot.Slider_mouse", require.toUrl("./robot/Slider_mouse.html"), 999999);
		doh.register("form.robot.Slider_a11y", require.toUrl("./robot/Slider_a11y.html"), 999999);
	}

	if(test_robot){
		doh.register("form.robot.Spinner_mouse", require.toUrl("./robot/Spinner_mouse.html"), 999999);
		doh.register("form.robot.Spinner_a11y", require.toUrl("./robot/Spinner_a11y.html"), 999999);
	}

	doh.register("form.ExpandingTextAreaMixin", require.toUrl("./ExpandingTextAreaMixin.html"), 999999);
	if(test_robot){
		doh.register("form.robot.Textarea", require.toUrl("./robot/Textarea.html"), 999999);
	}

	if(test_robot){
		doh.register("form.robot.validationMessages", require.toUrl("./robot/validationMessages.html"), 999999);
	}

	doh.register("form.verticalAlign", require.toUrl("./test_verticalAlign.html"), 999999);

	doh.register("form.TextBox_types", require.toUrl("./TextBox_types.html"), 999999);

	doh.register("form.TextBox_sizes.tundra.ltr", require.toUrl("./TextBox_sizes.html?theme=tundra&dir=ltr"), 999999);
	doh.register("form.TextBox_sizes.tundra.rtl", require.toUrl("./TextBox_sizes.html?theme=tundra&dir=rtl"), 999999);
	doh.register("form.TextBox_sizes.tundra.quirks", require.toUrl("../quirks.html?file=form/TextBox_sizes.html&theme=tundra&dir=ltr"), 999999);
	doh.register("form.TextBox_sizes.claro.ltr", require.toUrl("./TextBox_sizes.html?theme=claro&dir=ltr"), 999999);
	doh.register("form.TextBox_sizes.claro.rtl", require.toUrl("./TextBox_sizes.html?theme=claro&dir=rtl"), 999999);
	doh.register("form.TextBox_sizes.claro.quirks", require.toUrl("../quirks.html?file=form/TextBox_sizes.html&theme=claro&dir=ltr"), 999999);
	doh.register("form.TextBox_sizes.soria.ltr", require.toUrl("./TextBox_sizes.html?theme=soria&dir=ltr"), 999999);
	doh.register("form.TextBox_sizes.soria.rtl", require.toUrl("./TextBox_sizes.html?theme=soria&dir=rtl"), 999999);
	doh.register("form.TextBox_sizes.soria.quirks", require.toUrl("../quirks.html?file=form/TextBox_sizes.html&theme=soria&dir=rtl"), 999999);
	doh.register("form.TextBox_sizes.nihilo.ltr", require.toUrl("./TextBox_sizes.html?theme=nihilo&dir=ltr"), 999999);
	doh.register("form.TextBox_sizes.nihilo.rtl", require.toUrl("./TextBox_sizes.html?theme=nihilo&dir=rtl"), 999999);
	doh.register("form.TextBox_sizes.nihilo.quirks", require.toUrl("../quirks.html?file=form/TextBox_sizes.html&theme=nihilo&dir=rtl"), 999999);
	doh.register("form.TextBox_sizes.a11y.ltr", require.toUrl("./TextBox_sizes.html?a11y=1&dir=ltr"), 999999);
	doh.register("form.TextBox_sizes.a11y.rtl", require.toUrl("./TextBox_sizes.html?a11y=1&dir=rtl"), 999999);
	doh.register("form.TextBox_sizes.a11y.quirks", require.toUrl("../quirks.html?file=form/TextBox_sizes.html&a11y=1&dir=ltr"), 999999);

});
